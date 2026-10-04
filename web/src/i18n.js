// SPDX-License-Identifier: GPL-3.0-only
export const appDescriptionsEs = {
    conways: "Juego de la vida con patrones que se colocan al tocar.",
    fluid: "Agua que se mueve al inclinar la placa.",
    miso: "Una mascota del bosque que responde al tacto y al movimiento.",
    lumen: "Un vuelo entre estrellas y constelaciones.",
    dungeon: "Exploración y combate autónomos en una mazmorra.",
    maze: "Exploración en primera persona de un laberinto generado.",
    wayfarer: "Una cabina con mundos y tráfico espacial.",
    threebody:
        "Ocho encuentros de tres soles luminosos, siempre en movimiento.",
    crt: "Una consola de estación ficticia con brillo de fósforo.",
};

export const messages = {
    en: {
        title: "ESP32 Pin Launcher · Install your apps",
        eyebrow: "9 APPS · ONE ESP32",
        heading: "Games and scenes<br><span>for your pin.</span>",
        intro: "Pick your apps. Connect your pin.<br>Install the collection from your browser.",
        choose: '<span class="step">01</span> Choose your apps',
        installHeading: '<span class="step">02</span> Install on your pin',
        selectAll: "Select all",
        clear: "Clear selection",
        loadingApps: "Loading the app collection…",
        includedHeading: "Launcher menu + Demo are always included",
        included:
            "Your menu shows the apps you select. Demo cycles through them for five minutes each, then repeats.",
        browserStep: "Open this page in desktop Chrome or Edge.",
        cableStep: "Connect your Waveshare pin with a USB data cable.",
        portStep: "Click below and select the device’s USB serial port.",
        collection: "Your collection",
        launcher: "Launcher & Demo",
        includedLabel: "Included",
        downloadLabel: "Download",
        replace:
            "Installing replaces the firmware and resets on-device settings.",
        connect: 'Connect & install <span aria-hidden="true">↗</span>',
        progress: "Installation progress",
        loading: "Loading firmware release…",
        log: "Installation log",
        controlsHeading: "After installation",
        menuControls:
            "Tap <strong>BOOT</strong> to move through the menu. Hold for <strong>0.7 seconds</strong>, then release to launch.",
        appControls:
            "Inside any app, hold <strong>BOOT for 1.5 seconds</strong> and release to return to the menu or stop Demo.",
        helpHeading: "Device won’t connect?",
        helpPort:
            "Close other serial monitors. Hold BOOT, press and release RESET, then release BOOT. Try installing again and select the newly appeared port.",
        helpCable:
            "If it still fails, try another USB data cable or port. Keep the pin connected until installation finishes.",
        offline: "Runs on your pin. No account required.",
        thirdParty: "Third-party licenses",
        credits: "Credits",
        source: "Source code",
        firmwareLicenses: "Firmware licenses",
        selectionOne: "{count} app",
        selectionMany: "{count} apps",
        ready: "Ready when you are.",
        empty: "Choose at least one app to install.",
        installApp: "Install {name}",
        build: "FIRMWARE BUILD {build}",
        picker: "Select your pin’s USB serial port…",
        downloading: "Downloading and checking your firmware…",
        connecting: "Connecting to your pin…",
        writing: "Installing and verifying… Keep your pin connected.",
        successOne: "{count} app installed and verified.",
        successMany: "{count} apps installed and verified.",
        resetAuto: "Your pin will open the launcher. If needed, press RESET.",
        resetManual: "Press RESET to open the launcher.",
        canceled:
            "No device selected. Click Connect & install when you’re ready.",
        failed: "Installation stopped: {error} Use the connection help below, then retry.",
        unsupported:
            "Use desktop Chrome or Edge to install over USB. You can browse the apps here.",
        insecure:
            "Open this page over HTTPS or localhost to enable USB installation.",
        unavailable: "Firmware release is unavailable. Please try again later.",
        loadFailed: "The firmware collection could not be loaded.",
        invalidManifest: "Invalid firmware manifest. Rebuild the web release.",
        invalidCapacity: "Invalid firmware partition capacity.",
        incompatible: "This firmware release is incompatible with the flasher.",
        bootloaderOverlap: "Bootloader overlaps the partition table.",
        invalidCatalog: "Invalid app catalog.",
        unknownSelection: "Unknown app selection.",
        overflow: "Selection exceeds the board's 16 MB flash.",
        integrity:
            "Firmware integrity check failed. Reload the page and try again.",
        downloadFailed:
            "Firmware download failed. Check your connection and try again.",
        wrongChip:
            "Wrong device: this installer requires an ESP32-S3 Waveshare AMOLED 1.91″ pin.",
        wrongFlash:
            "Wrong flash size: this pin requires 16 MB. Nothing was written.",
    },
    es: {
        title: "ESP32 Pin Launcher · Instalá tus apps",
        eyebrow: "9 APPS · UN ESP32",
        heading: "Juegos y escenas<br><span>para tu pin.</span>",
        intro: "Elegí tus apps. Conectá tu pin.<br>Instalá la colección desde el navegador.",
        choose: '<span class="step">01</span> Elegí tus apps',
        installHeading: '<span class="step">02</span> Instalá en tu pin',
        selectAll: "Elegir todas",
        clear: "Quitar selección",
        loadingApps: "Cargando la colección de apps…",
        includedHeading: "El menú y Demo se incluyen siempre",
        included:
            "El menú muestra las apps que elegís. Demo las recorre durante cinco minutos cada una y vuelve a empezar.",
        browserStep: "Abrí esta página en Chrome o Edge de escritorio.",
        cableStep: "Conectá tu Waveshare con un cable USB de datos.",
        portStep:
            "Presioná el botón y elegí el puerto serie USB del dispositivo.",
        collection: "Tu colección",
        launcher: "Menú y Demo",
        includedLabel: "Incluidos",
        downloadLabel: "Descarga",
        replace:
            "Instalar reemplaza el firmware y reinicia los ajustes de la placa.",
        connect: 'Conectar e instalar <span aria-hidden="true">↗</span>',
        progress: "Progreso de instalación",
        loading: "Cargando la versión de firmware…",
        log: "Registro de instalación",
        controlsHeading: "Después de instalar",
        menuControls:
            "Tocá <strong>BOOT</strong> para cambiar la selección. Mantenelo <strong>0,7 segundos</strong> y soltalo para iniciar.",
        appControls:
            "Dentro de una app, mantené <strong>BOOT 1,5 segundos</strong> y soltalo para volver al menú o detener Demo.",
        helpHeading: "¿No conecta el dispositivo?",
        helpPort:
            "Cerrá otros monitores serie. Mantené BOOT, presioná y soltá RESET, y soltá BOOT. Reintentá la instalación y elegí el puerto que aparece.",
        helpCable:
            "Si sigue fallando, probá otro cable USB de datos o puerto. Dejá la placa conectada hasta que termine la instalación.",
        offline: "Funciona en tu pin. Sin cuenta.",
        thirdParty: "Licencias de terceros",
        credits: "Créditos",
        source: "Código fuente",
        firmwareLicenses: "Licencias del firmware",
        selectionOne: "{count} app",
        selectionMany: "{count} apps",
        ready: "Listo para instalar.",
        empty: "Elegí al menos una app para instalar.",
        installApp: "Instalar {name}",
        build: "VERSIÓN DE FIRMWARE {build}",
        picker: "Elegí el puerto serie USB de tu pin…",
        downloading: "Descargando y comprobando el firmware…",
        connecting: "Conectando al pin…",
        writing: "Instalando y verificando… Mantené el pin conectado.",
        successOne: "{count} app instalada y verificada.",
        successMany: "{count} apps instaladas y verificadas.",
        resetAuto: "El pin abrirá el menú. Si hace falta, presioná RESET.",
        resetManual: "Presioná RESET para abrir el menú.",
        canceled:
            "No elegiste un dispositivo. Presioná Conectar e instalar cuando estés listo.",
        failed: "Se detuvo la instalación: {error} Consultá la ayuda de conexión y reintentá.",
        unsupported:
            "Usá Chrome o Edge de escritorio para instalar por USB. Podés ver las apps aquí.",
        insecure:
            "Abrí esta página por HTTPS o localhost para habilitar la instalación USB.",
        unavailable:
            "El firmware no está disponible. Volvé a intentarlo más tarde.",
        loadFailed: "No se pudo cargar la colección de firmware.",
        invalidManifest:
            "Manifiesto de firmware inválido. Regenerá la versión web.",
        invalidCapacity: "Capacidad de partición inválida.",
        incompatible:
            "Esta versión de firmware no es compatible con el instalador.",
        bootloaderOverlap:
            "El bootloader se superpone a la tabla de particiones.",
        invalidCatalog: "Catálogo de apps inválido.",
        unknownSelection: "Selección de apps desconocida.",
        overflow: "La selección supera los 16 MB de flash de la placa.",
        integrity:
            "Falló la comprobación de integridad. Recargá la página y reintentá.",
        downloadFailed:
            "Falló la descarga del firmware. Revisá la conexión y reintentá.",
        wrongChip:
            "Dispositivo incorrecto: se necesita una Waveshare ESP32-S3 AMOLED de 1,91 pulgadas.",
        wrongFlash:
            "Memoria flash incorrecta: se necesitan 16 MB. No se escribió nada.",
    },
};

export function translate(language, key, values = {}) {
    return messages[language][key].replace(/\{(\w+)\}/g, (_, name) =>
        String(values[name]),
    );
}
