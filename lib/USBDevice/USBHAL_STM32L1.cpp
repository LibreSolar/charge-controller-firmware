/* Copyright (c) 2010-2015 mbed.org, MIT License
*
* Permission is hereby granted, free of charge, to any person obtaining a copy of this software
* and associated documentation files (the "Software"), to deal in the Software without
* restriction, including without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or
* substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
* BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
* DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#if defined(TARGET_STM32L1)||defined(TARGET_STM32F1)||defined(TARGET_STM32F0)
#include "USBDevice.h"

#if defined(TARGET_STM32F1)
#define USB_LP_IRQn USB_LP_CAN1_RX0_IRQn
const uint8_t PCD_EP_TYPE_CTRL = EP_TYPE_CTRL;
const uint8_t PCD_EP_TYPE_INTR = EP_TYPE_INTR;
const uint8_t PCD_EP_TYPE_BULK = EP_TYPE_BULK;
const uint8_t PCD_EP_TYPE_ISOC = EP_TYPE_ISOC;

#elif defined(TARGET_STM32L1)
void HAL_PCDEx_SetConnectionState(PCD_HandleTypeDef *hpcd, uint8_t state) {
    __SYSCFG_CLK_ENABLE(); // for SYSCFG_PMC_USB_PU
    if (state == 1) {
        __HAL_SYSCFG_USBPULLUP_ENABLE();
    } else {
        __HAL_SYSCFG_USBPULLUP_DISABLE();
    }
}

#elif defined(TARGET_STM32L0)||defined(TARGET_STM32F0)
#define USB_LP_IRQn USB_IRQn
void HAL_PCDEx_SetConnectionState(PCD_HandleTypeDef *hpcd, uint8_t state) {
    if (state == 1) {
        SET_BIT(USB->BCDR, USB_BCDR_DPPU); // DP Pull-up
    } else {
        CLEAR_BIT(USB->BCDR, USB_BCDR_DPPU);
    }
}

#elif defined(TARGET_STM32F3)
#define USB_LP_IRQn USB_LP_CAN1_RX0_IRQn
#endif

static PCD_HandleTypeDef hpcd_USB_FS;
static volatile int epComplete = 0;

USBHAL * USBHAL::instance;
uint32_t USBHAL::endpointReadcore(uint8_t endpoint, uint8_t *buffer) {return 0;}

USBHAL::USBHAL(void) {
    hpcd_USB_FS.pData = this;
    hpcd_USB_FS.Instance = USB;
    hpcd_USB_FS.Init.dev_endpoints = 8;
    hpcd_USB_FS.Init.speed = PCD_SPEED_FULL;
    hpcd_USB_FS.Init.ep0_mps = DEP0CTL_MPS_8;
    hpcd_USB_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
    hpcd_USB_FS.Init.Sof_enable = DISABLE;
    hpcd_USB_FS.Init.low_power_enable = DISABLE;
    hpcd_USB_FS.Init.battery_charging_enable = DISABLE;
    NVIC_SetVector(USB_LP_IRQn, (uint32_t)&_usbisr);
    HAL_PCD_Init(&hpcd_USB_FS);
    HAL_PCD_Start(&hpcd_USB_FS);
}

void HAL_PCD_MspInit(PCD_HandleTypeDef* hpcd) {
    //__USB_CLK_ENABLE();
    __HAL_RCC_USB_CLK_ENABLE();
    HAL_NVIC_SetPriority(USB_LP_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USB_LP_IRQn);
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef* hpcd) {
    //__USB_CLK_DISABLE(); // Peripheral clock disable
    __HAL_RCC_USB_CLK_DISABLE();
    HAL_NVIC_DisableIRQ(USB_LP_IRQn); // Peripheral interrupt Deinit
}

USBHAL::~USBHAL(void) {
    HAL_PCD_DeInit(&hpcd_USB_FS);
}

void USBHAL::connect(void) {
    HAL_PCD_DevConnect(&hpcd_USB_FS);
}

void USBHAL::disconnect(void) {
    HAL_PCD_DevDisconnect(&hpcd_USB_FS);
}

void USBHAL::configureDevice(void) {
    // Not needed
}
void USBHAL::unconfigureDevice(void) {
    // Not needed
}

void USBHAL::setAddress(uint8_t address) {
    HAL_PCD_SetAddress(&hpcd_USB_FS, address);
}

class PacketBufferAreaManager {
public:
    PacketBufferAreaManager(int bufsize_):bufsize(bufsize_) {
        reset();
    }
    void reset() {
        head = 0;
        tail = bufsize;
    }
    int allocBuf(int maxPacketSize) {
        head += 4;
        tail -= maxPacketSize;
        if (tail < head) {
            return 0;
        }
        return tail;
    }
private:
    int head,tail;
    int bufsize;
} PktBufArea(512);

bool USBHAL::realiseEndpoint(uint8_t endpoint, uint32_t maxPacket, uint32_t flags) {
    int pmaadress = PktBufArea.allocBuf(maxPacket);
    MBED_ASSERT(pmaadress != 0);
    if (pmaadress == 0) {
        return false;
    }
    PCD_HandleTypeDef *hpcd = &hpcd_USB_FS;
    uint8_t ep_type;
    switch(endpoint) {
        case EP0OUT:
            HAL_PCDEx_PMAConfig(hpcd, 0x00, PCD_SNG_BUF, pmaadress);
            HAL_PCD_EP_Open(hpcd, 0x00, maxPacket, PCD_EP_TYPE_CTRL);
            break;
        case EP0IN:
            HAL_PCDEx_PMAConfig(hpcd, 0x80, PCD_SNG_BUF, pmaadress);
            HAL_PCD_EP_Open(hpcd, 0x80, maxPacket, PCD_EP_TYPE_CTRL);
            break;
        case EPINT_OUT:
            HAL_PCDEx_PMAConfig(hpcd, 0x01, PCD_SNG_BUF, pmaadress);
            HAL_PCD_EP_Open(hpcd, 0x01, maxPacket, PCD_EP_TYPE_INTR);
            break;
        case EPINT_IN:
            HAL_PCDEx_PMAConfig(hpcd, 0x81, PCD_SNG_BUF, pmaadress);
            HAL_PCD_EP_Open(hpcd, 0x81, maxPacket, PCD_EP_TYPE_INTR);
            break;
        case EPBULK_OUT:
            HAL_PCDEx_PMAConfig(hpcd, 0x02, PCD_SNG_BUF, pmaadress);
            HAL_PCD_EP_Open(hpcd, 0x02, maxPacket, PCD_EP_TYPE_BULK);
            break;
        case EPBULK_IN:
            HAL_PCDEx_PMAConfig(hpcd, 0x82, PCD_SNG_BUF, pmaadress);
            HAL_PCD_EP_Open(hpcd, 0x82, maxPacket, PCD_EP_TYPE_BULK);
            break;
        case EP3OUT:
            HAL_PCDEx_PMAConfig(hpcd, 0x03, PCD_SNG_BUF, pmaadress);
            ep_type = (flags & ISOCHRONOUS) ? PCD_EP_TYPE_ISOC : PCD_EP_TYPE_BULK;
            HAL_PCD_EP_Open(hpcd, 0x03, maxPacket, ep_type);
            break;
        case EP3IN:
            HAL_PCDEx_PMAConfig(hpcd, 0x83, PCD_SNG_BUF, pmaadress);
            ep_type = (flags & ISOCHRONOUS) ? PCD_EP_TYPE_ISOC : PCD_EP_TYPE_BULK;
            HAL_PCD_EP_Open(hpcd, 0x83, maxPacket, ep_type);
            break;
        default:
            MBED_ASSERT(0);
            return false;
    }
    return true;
}

// read setup packet
void USBHAL::EP0setup(uint8_t *buffer) {
    memcpy(buffer, hpcd_USB_FS.Setup, 8);
}

void USBHAL::EP0readStage(void) {
}

void USBHAL::EP0read(void) {
    endpointRead(EP0OUT, MAX_PACKET_SIZE_EP0);
}

class rxTempBufferManager {
    uint8_t buf0[MAX_PACKET_SIZE_EP0];
    uint8_t buf1[MAX_PACKET_SIZE_EP1];
    uint8_t buf2[MAX_PACKET_SIZE_EP2];
    uint8_t buf3[MAX_PACKET_SIZE_EP3_ISO];
public:
    uint8_t* ptr(uint8_t endpoint, int maxPacketSize) {
        switch(endpoint) {
            case EP0OUT:
                MBED_ASSERT(maxPacketSize <= MAX_PACKET_SIZE_EP0);
                break;
            case EP1OUT:
                MBED_ASSERT(maxPacketSize <= MAX_PACKET_SIZE_EP1);
                break;
            case EP2OUT:
                MBED_ASSERT(maxPacketSize <= MAX_PACKET_SIZE_EP2);
                break;
            case EP3OUT:
                MBED_ASSERT(maxPacketSize <= MAX_PACKET_SIZE_EP3_ISO);
                break;
        }
        return ptr(endpoint);
    }
    uint8_t* ptr(uint8_t endpoint) {
        switch(endpoint) {
            case EP0OUT: return buf0;
            case EP1OUT: return buf1;
            case EP2OUT: return buf2;
            case EP3OUT: return buf3;
        }
        MBED_ASSERT(0);
        return NULL;
    }
} rxtmp;

uint32_t USBHAL::EP0getReadResult(uint8_t *buffer) {
    const uint8_t endpoint = EP0OUT;
    uint32_t length = HAL_PCD_EP_GetRxCount(&hpcd_USB_FS, endpoint>>1);
    memcpy(buffer, rxtmp.ptr(endpoint), length);
    return length;
}

void USBHAL::EP0write(uint8_t *buffer, uint32_t size) {
    endpointWrite(EP0IN, buffer, size);
}

void USBHAL::EP0getWriteResult(void) {
}

void USBHAL::EP0stall(void) {
    // If we stall the out endpoint here then we have problems transferring
    // and setup requests after the (stalled) get device qualifier requests.
    // TODO: Find out if this is correct behavior, or whether we are doing
    // something else wrong
    stallEndpoint(EP0IN);
//    stallEndpoint(EP0OUT);
}

EP_STATUS USBHAL::endpointRead(uint8_t endpoint, uint32_t maximumSize) {
    HAL_PCD_EP_Receive(&hpcd_USB_FS, endpoint>>1, rxtmp.ptr(endpoint, maximumSize), maximumSize);
    epComplete &= ~(1 << endpoint);
    return EP_PENDING;
}

EP_STATUS USBHAL::endpointReadResult(uint8_t endpoint, uint8_t * buffer, uint32_t *bytesRead) {
    if (!(epComplete & (1 << endpoint))) {
        return EP_PENDING;
    }
    int len = HAL_PCD_EP_GetRxCount(&hpcd_USB_FS, endpoint>>1);
    memcpy(buffer, rxtmp.ptr(endpoint), len);
    *bytesRead = len;
    return EP_COMPLETED;
}

EP_STATUS USBHAL::endpointWrite(uint8_t endpoint, uint8_t *data, uint32_t size) {
    HAL_PCD_EP_Transmit(&hpcd_USB_FS, endpoint>>1, data, size);
    epComplete &= ~(1 << endpoint);
    return EP_PENDING;
}

EP_STATUS USBHAL::endpointWriteResult(uint8_t endpoint) {
    if (epComplete & (1 << endpoint)) {
        epComplete &= ~(1 << endpoint);
        return EP_COMPLETED;
    }
    return EP_PENDING;
}

void USBHAL::stallEndpoint(uint8_t endpoint) {
    PCD_HandleTypeDef *hpcd = &hpcd_USB_FS;
    switch(endpoint) {
        case EP0IN:
            HAL_PCD_EP_SetStall(hpcd, 0x80);
            break;
        case EP0OUT:
            HAL_PCD_EP_SetStall(hpcd, 0x00);
            break;
        default:
            break;
    }
}

void USBHAL::unstallEndpoint(uint8_t endpoint) {
}

bool USBHAL::getEndpointStallState(uint8_t endpoint) {
    return false;
}

void USBHAL::remoteWakeup(void) {}

void USBHAL::_usbisr(void) {
    HAL_PCD_IRQHandler(&hpcd_USB_FS);
}

void USBHAL::usbisr(void) {}

void USBHAL::SetupStageCallback() {
    EP0setupCallback();
}

void USBHAL::DataInStageCallback(uint8_t epnum) {
    switch(epnum) {
        case 0: // EP0IN
            EP0in();
            break;
        case 1:
            epComplete |= (1<<EP1IN);
            if (EP1_IN_callback()) {
                epComplete &= ~(1<<EP1IN);
            }
            break;
        case 2:
            epComplete |= (1<<EP2IN);
            if (EP2_IN_callback()) {
                epComplete &= ~(1<<EP2IN);
            }
            break;
        case 3:
            epComplete |= (1<<EP3IN);
            if (EP3_IN_callback()) {
                epComplete &= ~(1<<EP3IN);
            }
            break;
        default:
            MBED_ASSERT(0);
            break;
    }
}

void USBHAL::DataOutStageCallback(uint8_t epnum) {
    switch(epnum) {
        case 0: // EP0OUT
            if ((hpcd_USB_FS.Setup[0]&0x80) == 0x00) { // host to device ?
                EP0out();
            }
            break;
        case 1:
            epComplete |= (1<<EP1OUT);
            if (EP1_OUT_callback()) {
                epComplete &= ~(1<<EP1OUT);
            }
            break;
        case 2:
            epComplete |= (1<<EP2OUT);
            if (EP2_OUT_callback()) {
                epComplete &= ~(1<<EP2OUT);
            }
            break;
        case 3:
            epComplete |= (1<<EP3OUT);
            if (EP3_OUT_callback()) {
                epComplete &= ~(1<<EP3OUT);
            }
            break;
        default:
            MBED_ASSERT(0);
            break;
    }
}

void USBHAL::ResetCallback() {
    PktBufArea.reset();
    realiseEndpoint(EP0IN, MAX_PACKET_SIZE_EP0, 0);
    realiseEndpoint(EP0OUT, MAX_PACKET_SIZE_EP0, 0);
}

void USBHAL::SOFCallback() {
    SOF(hpcd_USB_FS.Instance->FNR & USB_FNR_FN);
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd) {
    reinterpret_cast<USBHAL*>(hpcd->pData)->SetupStageCallback();
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
    reinterpret_cast<USBHAL*>(hpcd->pData)->DataInStageCallback(epnum);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
    reinterpret_cast<USBHAL*>(hpcd->pData)->DataOutStageCallback(epnum);
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd) {
    reinterpret_cast<USBHAL*>(hpcd->pData)->ResetCallback();
}

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd) {
    reinterpret_cast<USBHAL*>(hpcd->pData)->SOFCallback();
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd) {
    if (hpcd->Init.low_power_enable) {
        SCB->SCR |= (uint32_t)((uint32_t)(SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk));
    }
}

#endif
