/*
 * Copyright (c) 2016, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mbed-drivers/mbed.h"

#include "uif-matrixlcd/MatrixLCD.h"
#include "UIFramework/UIFramework.h"
#include "UIFramework/UITextView.h"

#include "uif-ui-popup-alert-wrd/AlertView.h"

#include "message-center/MessageCenter.h"
#include "message-center-transport/MessageCenterSPIMaster.h"
#include "mbed-block/BlockDynamic.h"
#include "cborg/Cbor.h"

#include "wrd-utilities/SharedModules.h"

#include <string>

///////////////////////////////////////////////////////////////////////////////

#define TYPE_CONTROL                    1
#define VALUE_CONNECTION_PERIPHERAL     1
#define VALUE_CONNECTION_CENTRAL        3
#define VALUE_DISCONNECTION_PERIPHERAL  2
#define VALUE_DISCONNECTION_CENTRAL     4

#if YOTTA_CFG_HARDWARE_WEARABLE_REFERENCE_DESIGN_BLUETOOTH_LE_PRESENT
#define BLE_NAME     YOTTA_CFG_HARDWARE_WEARABLE_REFERENCE_DESIGN_BLUETOOTH_LE_SPI_NAME
#define BLE_CS       YOTTA_CFG_HARDWARE_WEARABLE_REFERENCE_DESIGN_BLUETOOTH_LE_SPI_CS
#define BLE_NRDY     YOTTA_CFG_HARDWARE_WEARABLE_REFERENCE_DESIGN_BLUETOOTH_LE_PIN_NRDY
// message center
static MessageCenterSPIMaster transport(BLE_NAME, BLE_CS, BLE_NRDY);
#endif

static uif::MatrixLCD lcd;
static SharedPointer<UIFramework> uiFramework;

static InterruptIn forwardButton(YOTTA_CFG_HARDWARE_WEARABLE_REFERENCE_DESIGN_BUTTON_PIN_FORWARD);

static SharedPointer<BlockStatic> sendBlock;
static const uint32_t alertTimeOnScreen = 5000;
static const int8_t FIELD_SIZE = 4;

/*****************************************************************************/

void sendTaskDone()
{
    // free block
    sendBlock = SharedPointer<BlockStatic>();
}

void receivedControl(BlockStatic block)
{
    Cborg decoder(block.getData(), block.getLength());

    uint32_t type = 0;
    uint32_t value = 0;

    decoder.at(0).getUnsigned(&type);
    decoder.at(1).getUnsigned(&value);

    if (type == TYPE_CONTROL)
    {
        if ((value == VALUE_CONNECTION_PERIPHERAL) || (value == VALUE_CONNECTION_CENTRAL))
        {
            sendBlock = SharedPointer<BlockStatic>(new BlockDynamic(4 * FIELD_SIZE + sizeof("Bluetooth") + sizeof("Connected")));
            Cbore encoder(sendBlock->getData(), sendBlock->getLength());

            encoder.array(3)
                   .item(alertTimeOnScreen)
                   .item("Bluetooth")
                   .item("Connected");

            sendBlock->setLength(encoder.getLength());

            MessageCenter::sendTask(MessageCenter::LocalHost,
                                    MessageCenter::AlertPort,
                                    *sendBlock.get(),
                                    sendTaskDone);
        }
        else if ((value == VALUE_DISCONNECTION_PERIPHERAL) || (value == VALUE_DISCONNECTION_CENTRAL))
        {
            sendBlock = SharedPointer<BlockStatic>(new BlockDynamic(4 * FIELD_SIZE + sizeof("Bluetooth") + sizeof("Disconnected")));
            Cbore encoder(sendBlock->getData(), sendBlock->getLength());

            encoder.array(3)
                   .item(alertTimeOnScreen)
                   .item("Bluetooth")
                   .item("Disconnected");

            sendBlock->setLength(encoder.getLength());

            MessageCenter::sendTask(MessageCenter::LocalHost,
                                    MessageCenter::AlertPort,
                                    *sendBlock.get(),
                                    sendTaskDone);
        }
    }
}

void receivedEquip(BlockStatic block)
{
    Cborg decoder(block.getData(), block.getLength());

    uint32_t type = 0;

    decoder.at(0).getUnsigned(&type);

    if (type == TYPE_CONTROL)
    {
        std::string deviceName;
        decoder.at(1).getString(deviceName);

        sendBlock = SharedPointer<BlockStatic>(new BlockDynamic(4 * FIELD_SIZE + sizeof("Device Name") + deviceName.length()));
        Cbore encoder(sendBlock->getData(), sendBlock->getLength());

        encoder.array(3)
               .item(alertTimeOnScreen)
               .item("Device Name")
               .item(deviceName.c_str(), deviceName.length());

        sendBlock->setLength(encoder.getLength());

        MessageCenter::sendTask(MessageCenter::LocalHost,
                                MessageCenter::AlertPort,
                                *sendBlock.get(),
                                sendTaskDone);
    }
}

void buttonPressISR()
{
    std::string body = "Pressed!";

    sendBlock = SharedPointer<BlockStatic>(new BlockDynamic(4 * FIELD_SIZE + sizeof("Button") + body.length()));
    Cbore encoder(sendBlock->getData(), sendBlock->getLength());

    encoder.array(3)
           .item(alertTimeOnScreen)
           .item("Button")
           .item(body.c_str(), body.length());

    sendBlock->setLength(encoder.getLength());

    MessageCenter::sendTask(MessageCenter::LocalHost,
                            MessageCenter::AlertPort,
                            *sendBlock.get(),
                            sendTaskDone);

}

/*****************************************************************************/

void app_start(int, char *[])
{
    /* message center */
#if YOTTA_CFG_HARDWARE_WEARABLE_REFERENCE_DESIGN_BLUETOOTH_LE_PRESENT
    MessageCenter::addTransportTask(MessageCenter::RemoteHost, &transport);
#endif

    MessageCenter::addListenerTask(MessageCenter::LocalHost,
                                   MessageCenter::ControlPort,
                                   receivedControl);

    MessageCenter::addListenerTask(MessageCenter::LocalHost,
                                   MessageCenter::EquipPort,
                                   receivedEquip);

    /* button */
    forwardButton.fall(buttonPressISR);

    /* UIFramework */
    SharedPointer<UIView> view(new AlertView());
    view->setWidth(128);
    view->setHeight(128);

    uiFramework = SharedPointer<UIFramework>(new UIFramework(lcd, view));
}
