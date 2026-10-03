// SPDX-License-Identifier: GPL-3.0-only
#include "sdcard.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "sdmmc_cmd.h"
#include "driver/usb_serial_jtag.h"

/*
 * This is the current Waveshare 04_SD_Card VersionControl_V2 mapping for the
 * SKU 28596/v0.2 board used by this project.  It is the SDMMC 1-bit path and
 * deliberately does not claim the display's GPIO47 clock.
 */
enum {
    SD_CLK = GPIO_NUM_9,
    SD_CMD = GPIO_NUM_42,
    SD_D0 = GPIO_NUM_8,
};

static const char *TAG = "sdcard";
static const char *const SD_MOUNT_POINT = "/sd";

enum {
    PROTOCOL_VERSION = 1,
    FRAME_SIZE = 16,
    MAX_FRAME_PAYLOAD = 4096,
    MAX_PATH = 512,
    MAX_ENTRY_NAME = 255,
    READ_CHUNK = 2048,
};

static const uint8_t FRAME_MAGIC[4] = {'M', 'P', 'F', 'S'};

enum frame_type {
    FRAME_REQUEST = 0,
    FRAME_RESPONSE = 1,
    FRAME_DATA = 2,
};

enum frame_flags {
    FRAME_FLAG_MORE = 1u << 0,
    FRAME_FLAG_END = 1u << 1,
};

enum operation {
    OP_HELLO = 1,
    OP_STAT = 2,
    OP_LIST = 3,
    OP_READ = 4,
    OP_WRITE_BEGIN = 5,
    OP_WRITE_DATA = 6,
    OP_WRITE_END = 7,
    OP_WRITE_ABORT = 8,
    OP_MKDIR = 9,
    OP_DELETE = 10,
};

enum sd_status {
    SD_STATUS_OK = 0,
    SD_STATUS_NOT_FOUND = 1,
    SD_STATUS_INVALID = 2,
    SD_STATUS_IO = 3,
    SD_STATUS_NO_CARD = 4,
    SD_STATUS_BUSY = 5,
    SD_STATUS_EXISTS = 6,
    SD_STATUS_NOT_EMPTY = 7,
    SD_STATUS_NO_SPACE = 8,
    SD_STATUS_PROTOCOL = 9,
};

typedef struct {
    FILE *file;
    uint64_t expected;
    uint64_t received;
    char target[MAX_PATH];
    char temp[MAX_PATH];
    bool active;
} write_session_t;

static sdmmc_card_t *card;
static bool mounted;
static write_session_t write_session;

static bool parser_active;
static uint8_t parser_header[FRAME_SIZE];
static size_t parser_header_used;
static uint8_t parser_payload[MAX_FRAME_PAYLOAD];
static uint32_t parser_payload_len;
static size_t parser_payload_used;

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t get_u64(const uint8_t *p)
{
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) {
        value |= (uint64_t)p[i] << (i * 8);
    }
    return value;
}

static void put_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) {
        p[i] = (uint8_t)(value >> (i * 8));
    }
}

static void put_u64(uint8_t *p, uint64_t value)
{
    for (unsigned i = 0; i < 8; ++i) {
        p[i] = (uint8_t)(value >> (i * 8));
    }
}

static bool usb_write_all(const void *data, size_t length)
{
    const uint8_t *bytes = data;
    while (length) {
        int written = usb_serial_jtag_write_bytes(bytes, length, pdMS_TO_TICKS(1000));
        if (written <= 0) {
            return false;
        }
        bytes += written;
        length -= (size_t)written;
    }
    return true;
}

static bool send_frame(uint8_t type, uint8_t operation, uint8_t flags, uint32_t status,
                       const void *payload, size_t length)
{
    if (length > MAX_FRAME_PAYLOAD) {
        return false;
    }
    uint8_t header[FRAME_SIZE] = {
        FRAME_MAGIC[0],   FRAME_MAGIC[1], FRAME_MAGIC[2], FRAME_MAGIC[3],
        PROTOCOL_VERSION, type,           operation,      flags,
    };
    put_u32(header + 8, status);
    put_u32(header + 12, (uint32_t)length);
    return usb_write_all(header, sizeof(header)) && (!length || usb_write_all(payload, length));
}

static const char *status_text(enum sd_status status)
{
    switch (status) {
    case SD_STATUS_OK:
        return "ok";
    case SD_STATUS_NOT_FOUND:
        return "not found";
    case SD_STATUS_INVALID:
        return "invalid path or request";
    case SD_STATUS_IO:
        return "I/O error";
    case SD_STATUS_NO_CARD:
        return "SD card is not mounted";
    case SD_STATUS_BUSY:
        return "another operation is active";
    case SD_STATUS_EXISTS:
        return "already exists";
    case SD_STATUS_NOT_EMPTY:
        return "directory is not empty";
    case SD_STATUS_NO_SPACE:
        return "not enough space";
    case SD_STATUS_PROTOCOL:
        return "protocol error";
    default:
        return "unknown error";
    }
}

static bool send_response(uint8_t operation, uint8_t flags, enum sd_status status,
                          const void *payload, size_t length)
{
    return send_frame(FRAME_RESPONSE, operation, flags, status, payload, length);
}

static bool send_status(uint8_t operation, enum sd_status status)
{
    const char *message = status_text(status);
    return send_response(operation, 0, status, message, strlen(message));
}

static enum sd_status errno_status(int error)
{
    switch (error) {
    case ENOENT:
        return SD_STATUS_NOT_FOUND;
    case EEXIST:
        return SD_STATUS_EXISTS;
    case ENOTEMPTY:
        return SD_STATUS_NOT_EMPTY;
    case ENOSPC:
        return SD_STATUS_NO_SPACE;
    case EINVAL:
    case ENAMETOOLONG:
    case ENOTDIR:
    case EISDIR:
        return SD_STATUS_INVALID;
    default:
        return SD_STATUS_IO;
    }
}

static enum sd_status card_status(void)
{
    if (!mounted || !card) {
        return SD_STATUS_NO_CARD;
    }
    return sdmmc_get_status(card) == ESP_OK ? SD_STATUS_OK : SD_STATUS_NO_CARD;
}

/* Convert an untrusted, SD-root-relative path into a VFS path. */
static bool build_path(const uint8_t *raw, size_t raw_length, char *output, size_t output_size,
                       bool allow_root)
{
    if (!raw || !output || raw_length >= MAX_PATH) {
        return false;
    }
    if (raw_length == 0 || (raw_length == 1 && raw[0] == '/')) {
        if (!allow_root) {
            return false;
        }
        if (snprintf(output, output_size, "%s", SD_MOUNT_POINT) >= (int)output_size) {
            return false;
        }
        return true;
    }

    size_t start = raw[0] == '/' ? 1 : 0;
    if (start == raw_length || raw[start] == '/') {
        return false;
    }
    size_t position = strlen(SD_MOUNT_POINT);
    if (position + 1 >= output_size) {
        return false;
    }
    memcpy(output, SD_MOUNT_POINT, position);
    output[position++] = '/';

    size_t component_start = start;
    for (size_t i = start; i <= raw_length; ++i) {
        if (i < raw_length) {
            const uint8_t c = raw[i];
            if (c < 32 || c == '\\' || c == ':') {
                return false;
            }
            if (c != '/') {
                continue;
            }
        }
        const size_t component_length = i - component_start;
        if (component_length == 0 || component_length > MAX_ENTRY_NAME) {
            return false;
        }
        if ((component_length == 1 && raw[component_start] == '.') ||
            (component_length == 2 && raw[component_start] == '.' &&
             raw[component_start + 1] == '.')) {
            return false;
        }
        if (position + component_length + (i < raw_length ? 1 : 0) >= output_size) {
            return false;
        }
        memcpy(output + position, raw + component_start, component_length);
        position += component_length;
        if (i < raw_length) {
            output[position++] = '/';
        }
        component_start = i + 1;
    }
    output[position] = 0;
    return true;
}

static bool parse_path(const uint8_t *payload, size_t length, char *path, size_t path_size,
                       size_t *used, bool allow_root)
{
    if (length < 2) {
        return false;
    }
    const uint16_t path_length = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
    if ((size_t)path_length + 2 > length) {
        return false;
    }
    if (!build_path(payload + 2, path_length, path, path_size, allow_root)) {
        return false;
    }
    if (used) {
        *used = (size_t)path_length + 2;
    }
    return true;
}

static bool child_path(const char *directory, const char *name, char *output, size_t output_size)
{
    const int written = snprintf(output, output_size, "%s/%s", directory, name);
    return written >= 0 && (size_t)written < output_size;
}

static int delete_tree(const char *path, unsigned depth)
{
    if (depth > 32) {
        errno = ELOOP;
        return -1;
    }
    struct stat info;
    if (stat(path, &info) != 0) {
        return -1;
    }
    if (!S_ISDIR(info.st_mode)) {
        return unlink(path);
    }

    DIR *directory = opendir(path);
    if (!directory) {
        return -1;
    }
    struct dirent *entry;
    int result = 0;
    while ((entry = readdir(directory)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
            continue;
        }
        char child[MAX_PATH];
        if (!child_path(path, entry->d_name, child, sizeof(child)) ||
            delete_tree(child, depth + 1) != 0) {
            result = -1;
            break;
        }
    }
    const int saved_error = errno;
    closedir(directory);
    if (result != 0) {
        errno = saved_error;
        return -1;
    }
    return rmdir(path);
}

static int make_directories(const char *path)
{
    char current[MAX_PATH];
    size_t length = strlen(path);
    if (length >= sizeof(current)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(current, path, length + 1);
    for (char *separator = current + strlen(SD_MOUNT_POINT) + 1; *separator; ++separator) {
        if (*separator != '/') {
            continue;
        }
        *separator = 0;
        if (mkdir(current, 0775) != 0 && errno != EEXIST) {
            return -1;
        }
        *separator = '/';
    }
    if (mkdir(current, 0775) != 0 && errno != EEXIST) {
        return -1;
    }
    struct stat info;
    return stat(current, &info) == 0 && S_ISDIR(info.st_mode) ? 0 : -1;
}

static void write_session_abort(void)
{
    if (write_session.file) {
        fclose(write_session.file);
    }
    if (write_session.temp[0]) {
        unlink(write_session.temp);
    }
    memset(&write_session, 0, sizeof(write_session));
}

static void handle_hello(void)
{
    /* A reconnect after a cable pull must not leave a partial write session
       blocking the next client forever. */
    if (write_session.active) {
        write_session_abort();
    }
    uint8_t info[12] = {PROTOCOL_VERSION, card_status() == SD_STATUS_OK ? 1u : 0u};
    put_u64(info + 4, card ? (uint64_t)card->csd.capacity : 0);
    send_response(OP_HELLO, 0, SD_STATUS_OK, info, sizeof(info));
}

static void handle_stat(const uint8_t *payload, size_t length)
{
    char path[MAX_PATH];
    size_t used;
    if (!parse_path(payload, length, path, sizeof(path), &used, true) || used != length) {
        send_status(OP_STAT, SD_STATUS_INVALID);
        return;
    }
    enum sd_status status = card_status();
    if (status != SD_STATUS_OK) {
        send_status(OP_STAT, status);
        return;
    }
    struct stat info;
    if (stat(path, &info) != 0) {
        send_status(OP_STAT, errno_status(errno));
        return;
    }
    uint8_t result[12] = {S_ISDIR(info.st_mode) ? 1u : 0u};
    put_u64(result + 4, S_ISDIR(info.st_mode) ? 0 : (uint64_t)info.st_size);
    send_response(OP_STAT, 0, SD_STATUS_OK, result, sizeof(result));
}

static void handle_list(const uint8_t *payload, size_t length)
{
    char path[MAX_PATH];
    size_t used;
    if (!parse_path(payload, length, path, sizeof(path), &used, true) || used != length) {
        send_status(OP_LIST, SD_STATUS_INVALID);
        return;
    }
    enum sd_status status = card_status();
    if (status != SD_STATUS_OK) {
        send_status(OP_LIST, status);
        return;
    }
    DIR *directory = opendir(path);
    if (!directory) {
        send_status(OP_LIST, errno_status(errno));
        return;
    }
    if (!send_response(OP_LIST, FRAME_FLAG_MORE, SD_STATUS_OK, NULL, 0)) {
        closedir(directory);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
            continue;
        }
        char child[MAX_PATH];
        struct stat info;
        if (!child_path(path, entry->d_name, child, sizeof(child)) || stat(child, &info) != 0) {
            status = errno_status(errno);
            break;
        }
        const size_t name_length = strlen(entry->d_name);
        if (name_length > MAX_ENTRY_NAME || name_length > UINT16_MAX - 11) {
            status = SD_STATUS_INVALID;
            break;
        }
        uint8_t record[11 + MAX_ENTRY_NAME];
        record[0] = S_ISDIR(info.st_mode) ? 1u : 0u;
        put_u64(record + 1, S_ISDIR(info.st_mode) ? 0 : (uint64_t)info.st_size);
        put_u16(record + 9, (uint16_t)name_length);
        memcpy(record + 11, entry->d_name, name_length);
        if (!send_frame(FRAME_DATA, OP_LIST, 0, SD_STATUS_OK, record, 11 + name_length)) {
            closedir(directory);
            return;
        }
    }
    closedir(directory);
    send_response(OP_LIST, FRAME_FLAG_END, status, NULL, 0);
}

static void handle_read(const uint8_t *payload, size_t length)
{
    char path[MAX_PATH];
    size_t used;
    if (!parse_path(payload, length, path, sizeof(path), &used, false) || used != length) {
        send_status(OP_READ, SD_STATUS_INVALID);
        return;
    }
    enum sd_status status = card_status();
    if (status != SD_STATUS_OK) {
        send_status(OP_READ, status);
        return;
    }
    struct stat info;
    if (stat(path, &info) != 0) {
        send_status(OP_READ, errno_status(errno));
        return;
    }
    if (S_ISDIR(info.st_mode)) {
        send_status(OP_READ, SD_STATUS_INVALID);
        return;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        send_status(OP_READ, errno_status(errno));
        return;
    }
    uint8_t metadata[8];
    put_u64(metadata, (uint64_t)info.st_size);
    if (!send_response(OP_READ, FRAME_FLAG_MORE, SD_STATUS_OK, metadata, sizeof(metadata))) {
        fclose(file);
        return;
    }
    uint8_t buffer[READ_CHUNK];
    while (true) {
        size_t received = fread(buffer, 1, sizeof(buffer), file);
        if (received && !send_frame(FRAME_DATA, OP_READ, 0, SD_STATUS_OK, buffer, received)) {
            fclose(file);
            return;
        }
        if (received < sizeof(buffer)) {
            if (ferror(file)) {
                status = SD_STATUS_IO;
            }
            break;
        }
    }
    fclose(file);
    send_response(OP_READ, FRAME_FLAG_END, status, NULL, 0);
}

static void handle_write_begin(const uint8_t *payload, size_t length)
{
    if (write_session.active) {
        send_status(OP_WRITE_BEGIN, SD_STATUS_BUSY);
        return;
    }
    char path[MAX_PATH];
    size_t used;
    if (!parse_path(payload, length, path, sizeof(path), &used, false) || length != used + 9) {
        send_status(OP_WRITE_BEGIN, SD_STATUS_INVALID);
        return;
    }
    enum sd_status status = card_status();
    if (status != SD_STATUS_OK) {
        send_status(OP_WRITE_BEGIN, status);
        return;
    }
    const uint64_t expected = get_u64(payload + used);
    const uint8_t mode = payload[used + 8];
    if (mode != 0) {
        send_status(OP_WRITE_BEGIN, SD_STATUS_INVALID);
        return;
    }
    if (snprintf(write_session.temp, sizeof(write_session.temp), "%s.mpfs.tmp", path) >=
        (int)sizeof(write_session.temp)) {
        memset(&write_session, 0, sizeof(write_session));
        send_status(OP_WRITE_BEGIN, SD_STATUS_INVALID);
        return;
    }
    unlink(write_session.temp);
    FILE *file = fopen(write_session.temp, "wb");
    if (!file) {
        const enum sd_status error = errno_status(errno);
        memset(&write_session, 0, sizeof(write_session));
        send_status(OP_WRITE_BEGIN, error);
        return;
    }
    write_session.file = file;
    write_session.expected = expected;
    write_session.received = 0;
    memcpy(write_session.target, path, strlen(path) + 1);
    write_session.active = true;
    send_status(OP_WRITE_BEGIN, SD_STATUS_OK);
}

static void handle_write_data(const uint8_t *payload, size_t length)
{
    if (!write_session.active || !write_session.file) {
        send_status(OP_WRITE_DATA, SD_STATUS_INVALID);
        return;
    }
    if ((uint64_t)length > write_session.expected - write_session.received) {
        write_session_abort();
        send_status(OP_WRITE_DATA, SD_STATUS_INVALID);
        return;
    }
    if (length && fwrite(payload, 1, length, write_session.file) != length) {
        const enum sd_status error = errno_status(errno);
        write_session_abort();
        send_status(OP_WRITE_DATA, error);
        return;
    }
    write_session.received += length;
    uint8_t acknowledged[8];
    put_u64(acknowledged, write_session.received);
    send_response(OP_WRITE_DATA, 0, SD_STATUS_OK, acknowledged, sizeof(acknowledged));
}

static void handle_write_end(void)
{
    if (!write_session.active || !write_session.file) {
        send_status(OP_WRITE_END, SD_STATUS_INVALID);
        return;
    }
    if (write_session.received != write_session.expected) {
        write_session_abort();
        send_status(OP_WRITE_END, SD_STATUS_INVALID);
        return;
    }
    const bool flushed = fflush(write_session.file) == 0;
    const bool closed = fclose(write_session.file) == 0;
    bool success = flushed && closed;
    write_session.file = NULL;
    enum sd_status status = success ? SD_STATUS_OK : SD_STATUS_IO;
    if (success && rename(write_session.temp, write_session.target) != 0) {
        status = errno_status(errno);
        unlink(write_session.temp);
    }
    memset(&write_session, 0, sizeof(write_session));
    send_status(OP_WRITE_END, status);
}

static void handle_mkdir(const uint8_t *payload, size_t length)
{
    char path[MAX_PATH];
    size_t used;
    if (!parse_path(payload, length, path, sizeof(path), &used, true) || length != used + 1) {
        send_status(OP_MKDIR, SD_STATUS_INVALID);
        return;
    }
    enum sd_status status = card_status();
    if (status != SD_STATUS_OK) {
        send_status(OP_MKDIR, status);
        return;
    }
    if (!strcmp(path, SD_MOUNT_POINT)) {
        send_status(OP_MKDIR, SD_STATUS_OK);
        return;
    }
    int result = payload[used] ? make_directories(path) : mkdir(path, 0775);
    if (result != 0 && errno == EEXIST) {
        struct stat info;
        if (stat(path, &info) == 0 && S_ISDIR(info.st_mode)) {
            result = 0;
        }
    }
    send_status(OP_MKDIR, result == 0 ? SD_STATUS_OK : errno_status(errno));
}

static void handle_delete(const uint8_t *payload, size_t length)
{
    char path[MAX_PATH];
    size_t used;
    if (!parse_path(payload, length, path, sizeof(path), &used, false) || length != used + 1) {
        send_status(OP_DELETE, SD_STATUS_INVALID);
        return;
    }
    enum sd_status status = card_status();
    if (status != SD_STATUS_OK) {
        send_status(OP_DELETE, status);
        return;
    }
    int result;
    if (payload[used]) {
        result = delete_tree(path, 0);
    } else {
        struct stat info;
        if (stat(path, &info) != 0) {
            result = -1;
        } else {
            result = S_ISDIR(info.st_mode) ? rmdir(path) : unlink(path);
        }
    }
    send_status(OP_DELETE, result == 0 ? SD_STATUS_OK : errno_status(errno));
}

static void handle_frame(uint8_t type, uint8_t operation, const uint8_t *payload, size_t length)
{
    if (type == FRAME_DATA && operation == OP_WRITE_DATA) {
        handle_write_data(payload, length);
        return;
    }
    if (type != FRAME_REQUEST) {
        send_status(operation, SD_STATUS_PROTOCOL);
        return;
    }
    switch (operation) {
    case OP_HELLO:
        if (!length) {
            handle_hello();
        } else {
            send_status(operation, SD_STATUS_INVALID);
        }
        break;
    case OP_STAT:
        handle_stat(payload, length);
        break;
    case OP_LIST:
        handle_list(payload, length);
        break;
    case OP_READ:
        handle_read(payload, length);
        break;
    case OP_WRITE_BEGIN:
        handle_write_begin(payload, length);
        break;
    case OP_WRITE_END:
        if (!length) {
            handle_write_end();
        } else {
            send_status(operation, SD_STATUS_INVALID);
        }
        break;
    case OP_WRITE_ABORT:
        if (length) {
            send_status(operation, SD_STATUS_INVALID);
        } else {
            write_session_abort();
            send_status(operation, SD_STATUS_OK);
        }
        break;
    case OP_MKDIR:
        handle_mkdir(payload, length);
        break;
    case OP_DELETE:
        handle_delete(payload, length);
        break;
    default:
        send_status(operation, SD_STATUS_PROTOCOL);
        break;
    }
}

static void parser_reset(void)
{
    parser_active = false;
    parser_header_used = 0;
    parser_payload_len = 0;
    parser_payload_used = 0;
}

bool sdcard_protocol_feed_byte(uint8_t byte)
{
    if (!parser_active) {
        if (byte != FRAME_MAGIC[0]) {
            return false;
        }
        parser_active = true;
        parser_header_used = 1;
        parser_header[0] = byte;
        return true;
    }
    if (parser_header_used < FRAME_SIZE) {
        parser_header[parser_header_used++] = byte;
        if (parser_header_used < FRAME_SIZE) {
            return true;
        }
        if (memcmp(parser_header, FRAME_MAGIC, sizeof(FRAME_MAGIC)) != 0 ||
            parser_header[4] != PROTOCOL_VERSION ||
            get_u32(parser_header + 12) > MAX_FRAME_PAYLOAD) {
            parser_reset();
            return true;
        }
        parser_payload_len = get_u32(parser_header + 12);
        parser_payload_used = 0;
        if (!parser_payload_len) {
            handle_frame(parser_header[5], parser_header[6], NULL, 0);
            parser_reset();
        }
        return true;
    }
    parser_payload[parser_payload_used++] = byte;
    if (parser_payload_used == parser_payload_len) {
        handle_frame(parser_header[5], parser_header[6], parser_payload, parser_payload_len);
        parser_reset();
    }
    return true;
}

bool sdcard_protocol_busy(void)
{
    return parser_active || write_session.active;
}

void sdcard_init(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 12,
        .allocation_unit_size = 512,
        .disk_status_check_enable = true,
    };
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1;
    slot.clk = SD_CLK;
    slot.cmd = SD_CMD;
    slot.d0 = SD_D0;

    const esp_err_t error =
        esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot, &mount_config, &card);
    if (error != ESP_OK) {
        card = NULL;
        ESP_LOGW(TAG, "SD mount unavailable: %s", esp_err_to_name(error));
        return;
    }
    mounted = true;
    ESP_LOGI(TAG, "SD mounted at %s using SDMMC 1-bit (CLK=%d CMD=%d D0=%d)", SD_MOUNT_POINT,
             SD_CLK, SD_CMD, SD_D0);
}
