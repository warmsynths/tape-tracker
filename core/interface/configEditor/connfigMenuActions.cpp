// Device settings that act on the hardware, called at startup and by the Four-Track Device settings
// page (core/ft, FourTrackHal). What is left of the stock Config screen.
#include <string.h>

#include "Platform.h"
#include "mtAudioEngine.h"
#include "mtStructs.h"
#include "configEditor/configMenuActions.h"
#include "display.h"
#include "core/interface.h"

// device startup actions after Config read
void initConfigValuesAfterReadConfig()
{
    usbAudioChange();
    setDisplayBrightness();
}

void setDisplayBrightness()
{
    if (mtConfig.general.dispBrightness > 2)
        mtConfig.general.dispBrightness = 2;

    switch (mtConfig.general.dispBrightness)
    {
        case 0: display.setBacklightBrightness(20); break;
        case 1: display.setBacklightBrightness(50); break;
        case 2: display.setBacklightBrightness(128); break;
        default: break;
    }
}

void setLineInOutType()
{
    if (mtConfig.general.lineInOutType > 2)
        mtConfig.general.lineInOutType = 2;
    engine.setLineInOutType(mtConfig.general.lineInOutType);
}

void usbAudioChange()
{
    Usb* usb = mtInterface.getPlatform()->getUsb();

    switch (mtConfig.common.usbAudioMode)
    {
        case 1:
            usb->setClassState(Usb::ClassType::kAudioIn, true);
            usb->setClassState(Usb::ClassType::kAudioOut, false);
            break;
        case 2:
            usb->setClassState(Usb::ClassType::kAudioIn, false);
            usb->setClassState(Usb::ClassType::kAudioOut, true);
            break;
        case 3:
            usb->setClassState(Usb::ClassType::kAudioIn, true);
            usb->setClassState(Usb::ClassType::kAudioOut, true);
            break;
        default:
            usb->setClassState(Usb::ClassType::kAudioIn, false);
            usb->setClassState(Usb::ClassType::kAudioOut, false);
            break;
    }
#ifdef PLATFORM_IMX
    engine.refreshOutputSourceOutsideTheRecorder();
#endif
    usb->reset();
}


void checkFuncitonKey(int key)
{
    for (int idx = 0; idx < 8; idx++)
    {
        if (idx != key && mtConfig.interface.functionKeysBinding[idx] ==
                            mtConfig.interface.functionKeysBinding[key])
        {
            int firstFreeFunction = 0;
            bool assigned;

            for (uint8_t functionIdx = 0; functionIdx < kMenuModulesCount; functionIdx++)
            {
                assigned = false;
                for (uint8_t keyIdx = 0; keyIdx < 8; keyIdx++)
                {
                    if (mtConfig.interface.functionKeysBinding[keyIdx] == functionIdx)
                    {
                        assigned = true;
                    }
                }

                if (!assigned)
                {
                    firstFreeFunction = functionIdx;
                    break;
                }
            }

            mtConfig.interface.functionKeysBinding[idx] = firstFreeFunction;
        }
    }
}
