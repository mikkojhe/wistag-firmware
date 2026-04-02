#include "SystemCommandsModule.h"
#include "input/InputBroker.h"
#include "meshUtils.h"

#if HAS_SCREEN
#include "MessageStore.h"
#include "graphics/Screen.h"
#include "graphics/SharedUIDisplay.h"
#endif

#include "GPS.h"
#include "MeshService.h"
#include "Module.h"
#include "NodeDB.h"
#include "mesh/Router.h"
#include "main.h"
#include "modules/AdminModule.h"
#include "modules/ExternalNotificationModule.h"

SystemCommandsModule *systemCommandsModule;

SystemCommandsModule::SystemCommandsModule()
{
    if (inputBroker)
        inputObserver.observe(inputBroker);
}

int SystemCommandsModule::handleInputEvent(const InputEvent *event)
{
    LOG_INPUT("SystemCommands Input event %u! kb %u", event->inputEvent, event->kbchar);
    // System commands (all others fall through)
    switch (event->kbchar) {
    // Fn key symbols
    case INPUT_BROKER_MSG_FN_SYMBOL_ON:
    case INPUT_BROKER_MSG_FN_SYMBOL_OFF:
        return 0;
    // Brightness
    case INPUT_BROKER_MSG_BRIGHTNESS_UP:
        IF_SCREEN(screen->increaseBrightness());
        LOG_DEBUG("Increase Screen Brightness");
        return 0;
    case INPUT_BROKER_MSG_BRIGHTNESS_DOWN:
        IF_SCREEN(screen->decreaseBrightness());
        LOG_DEBUG("Decrease Screen Brightness");
        return 0;
    // Mute
    case INPUT_BROKER_MSG_MUTE_TOGGLE:
        if (moduleConfig.external_notification.enabled && externalNotificationModule) {
            externalNotificationModule->setMute(!externalNotificationModule->getMute());
            IF_SCREEN(if (!externalNotificationModule->getMute()) externalNotificationModule->stopNow(); screen->showSimpleBanner(
                externalNotificationModule->getMute() ? "Notifications\nDisabled" : "Notifications\nEnabled", 3000);)
        }
        return 0;
    // Bluetooth
    case INPUT_BROKER_MSG_BLUETOOTH_TOGGLE:
        config.bluetooth.enabled = !config.bluetooth.enabled;
        LOG_INFO("User toggled Bluetooth");
        nodeDB->saveToDisk();
#if defined(ARDUINO_ARCH_NRF52)
        if (!config.bluetooth.enabled) {
            disableBluetooth();
            IF_SCREEN(screen->showSimpleBanner("Bluetooth OFF\nRebooting", 3000));
            rebootAtMsec = millis() + DEFAULT_REBOOT_SECONDS * 2000;
        } else {
            IF_SCREEN(screen->showSimpleBanner("Bluetooth ON\nRebooting", 3000));
            rebootAtMsec = millis() + DEFAULT_REBOOT_SECONDS * 1000;
        }
#else
        if (!config.bluetooth.enabled) {
            disableBluetooth();
            IF_SCREEN(screen->showSimpleBanner("Bluetooth OFF", 3000));
        } else {
            IF_SCREEN(screen->showSimpleBanner("Bluetooth ON\nRebooting", 3000));
            rebootAtMsec = millis() + DEFAULT_REBOOT_SECONDS * 1000;
        }
#endif
        return 0;
    case INPUT_BROKER_MSG_REBOOT:
        IF_SCREEN(screen->showSimpleBanner("Rebooting...", 0));
        nodeDB->saveToDisk();
#if HAS_SCREEN
        messageStore.saveToFlash();
#endif
        rebootAtMsec = millis() + DEFAULT_REBOOT_SECONDS * 1000;
        // runState = CANNED_MESSAGE_RUN_STATE_INACTIVE;
        return true;
    }

    switch (event->inputEvent) {
        // GPS
    case INPUT_BROKER_GPS_TOGGLE:
#if !MESHTASTIC_EXCLUDE_GPS
        if (gps) {
            if (config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED &&
                config.position.fixed_position == false) {
                nodeDB->clearLocalPosition();
                nodeDB->saveToDisk();
            }
            gps->toggleGpsMode();
            const char *msg =
                (config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED) ? "GPS Enabled" : "GPS Disabled";
            IF_SCREEN(screen->forceDisplay(); screen->showSimpleBanner(msg, 3000);)
        }
#endif
        return true;
    // Mesh ping
    case INPUT_BROKER_SEND_PING:
        service->refreshLocalMeshNode();
        if (service->trySendPosition(NODENUM_BROADCAST, true)) {
            IF_SCREEN(screen->showSimpleBanner("Position\nSent", 3000));
        } else {
            IF_SCREEN(screen->showSimpleBanner("Node Info\nSent", 3000));
        }
        return true;
    case INPUT_BROKER_SEND_TIC: {
        static const char predefinedMessage[] = "TIC";
        meshtastic_MeshPacket *p = router ? router->allocForSending() : nullptr;
        if (!p) {
            LOG_ERROR("Unable to allocate packet for TIC message");
            return true;
        }

        p->to = NODENUM_BROADCAST;
        p->channel = 0;
        p->want_ack = false;
        p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
        p->decoded.payload.size = sizeof(predefinedMessage) - 1;
        memcpy(p->decoded.payload.bytes, predefinedMessage, p->decoded.payload.size);

        service->sendToMesh(p, RX_SRC_LOCAL, true);
        service->refreshLocalMeshNode();
        service->trySendPosition(NODENUM_BROADCAST, true);
        LOG_INFO("Sent predefined TIC broadcast message with position");
        IF_SCREEN(screen->showSimpleBanner("TIC\nSent", 3000));
        return true;
    }
    case INPUT_BROKER_SEND_MEDEVAC: {
        static const char medevacMessage[] = "MEDEVAC";
        meshtastic_MeshPacket *p = router ? router->allocForSending() : nullptr;
        if (!p) {
            LOG_ERROR("Unable to allocate packet for MEDEVAC message");
            return true;
        }

        p->to = NODENUM_BROADCAST;
        p->channel = 0;
        p->want_ack = false;
        p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
        p->decoded.payload.size = sizeof(medevacMessage) - 1;
        memcpy(p->decoded.payload.bytes, medevacMessage, p->decoded.payload.size);

        service->sendToMesh(p, RX_SRC_LOCAL, true);
        service->refreshLocalMeshNode();
        service->trySendPosition(NODENUM_BROADCAST, true);
        LOG_INFO("Sent predefined MEDEVAC broadcast message with position");
        IF_SCREEN(screen->showSimpleBanner("MEDEVAC\nSent", 3000));
        return true;
    }
    // Power control
    case INPUT_BROKER_SHUTDOWN:
        shutdownAtMsec = millis();
        return true;

    default:
        // No other input events handled here
        break;
    }
    return false;
}
