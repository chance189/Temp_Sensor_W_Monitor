/******************************************************************************
*
*Author: Chance
*Purpose: To read data from a temperature sensor, displaying the value
*on a 7 Segment Display, and send the value over UART to a monitoring GUI
*The design uses a lot of example code from Xilinx
*****************************************************************************/

/***************************** Include Files ********************************/

#include "xparameters.h"
#include "xiic.h"
#include "xintc.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xuartlite.h"
#include "xgpio_l.h"

/************************** Constant Definitions *****************************/

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are defined here such that a user can easily
 * change all the needed parameters in one place.
 */
#define UARTLITE_DEVICE_ID      XPAR_UARTLITE_0_DEVICE_ID
#define IIC_DEVICE_ID			XPAR_IIC_0_DEVICE_ID
#define INTC_DEVICE_ID			XPAR_INTC_0_DEVICE_ID
#define INTC_IIC_INTERRUPT_ID	XPAR_INTC_0_IIC_0_VEC_ID
#define UARTLITE_INT_IRQ_ID     XPAR_INTC_0_UARTLITE_0_VEC_ID
#define SEVEN_SEG_BASE_REG		XPAR_AXI_GPIO_0_BASEADDR


/*
 * Address for the ADT7420 Sensor
 */
#define TEMP_SENSOR_ADDRESS	0x4B

#define TEST_BUFFER_SIZE    500
#define LED_CHANNEL			1


/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ****************************/

int SetupUartLite_IIC(u16 DeviceId, u16 IicDeviceId, u8 TempSensorAddress);

static int SetupInterruptSystem(XIic *IicPtr, XUartLite *UartLitePtr);

static void RecvHandlerIIC(void *CallbackRef, int ByteCount);

static void StatusHandlerIIC(void *CallbackRef, int Status);

void SendHandlerUART(void *CallBackRef, unsigned int EventData);

void RecvHandlerUART(void *CallBackRef, unsigned int EventData);

static int SevenSegValue(u32 LED_Value);


/************************** Variable Definitions **************************/

XIic Iic;		  /* The instance of the IIC device */

XIntc InterruptController;  /* The instance of the Interrupt controller */

XUartLite UartLite; //Instance of UartLite Device
XUartLite_Config *UartLite_Cfg; //For configuration of UART


/*
 * The following structure contains fields that are used with the callbacks
 * (handlers) of the IIC driver. The driver asynchronously calls handlers
 * when abnormal events occur or when data has been sent or received. This
 * structure must be volatile to work when the code is optimized.
 */
volatile struct {
	int  EventStatus;
	int  RemainingRecvBytes;
	int EventStatusUpdated;
	int RecvBytesUpdated;
} HandlerInfo;


/*
 * The following buffers are used in this example to send and receive data
 * with the UartLite.
 */
u8 SendBuffer[TEST_BUFFER_SIZE];
u8 ReceiveBuffer[TEST_BUFFER_SIZE];

/* Here are the pointers to the buffer */
u8* ReceiveBufferPtr = &ReceiveBuffer[0];
u8* CommandPtr = &ReceiveBuffer[0];

static volatile int TotalReceivedCount; //volatile is used so that values are not lost
static volatile int TotalSentCount;

int main(void)
{
	long i = 0;
	int Status;
	u8 TemperaturePtr[2];
	uint16_t temp;

	//Setup Uart
	Status = SetupUartLite_IIC(UARTLITE_DEVICE_ID, IIC_DEVICE_ID, TEMP_SENSOR_ADDRESS);
	if (Status != XST_SUCCESS) {
		//xil_printf("FAILURE UART OR IIC\n\r");
		return XST_FAILURE;
	}
	//xil_printf("SUCCESSFULLY SET UP UART AND IIC\n\r");
	//Get reference to configuration
	UartLite_Cfg = XUartLite_LookupConfig(UARTLITE_DEVICE_ID);

	/*
	 * This is the event loop we should never return from
	 * Here will will send out the temperature sensor every second
	 * And display the approximate value in Celsius to the 7 Segment Display
	 */
	while(i == 0)
	{
		for(int k = 0; k < 50000; k++){}
		/*
		* Clear updated flags such that they can be polled to indicate
		* when the handler information has changed asynchronously and
		* initialize the status which will be returned to a default value
		*/
	    HandlerInfo.EventStatusUpdated = FALSE;
		HandlerInfo.RecvBytesUpdated = FALSE;
		Status = XST_FAILURE;

		/*
		* Attempt to receive a byte of data from the temperature sensor
		* on the IIC interface, ignore the return value since this example is
		* a single master system such that the IIC bus should not ever be busy
		*/
		(void)XIic_MasterRecv(&Iic, TemperaturePtr, 2);

		/*
		* The message is being received from the temperature sensor,
		* wait for it to complete by polling the information that is
		* updated asynchronously by interrupt processing
		*/
	    while(1)
	    {
	    	if(HandlerInfo.RecvBytesUpdated == TRUE) {
	    		/*
	    		 * The device information has been updated for receive
	    		 * processing,if all bytes received (2), indicate
	    		 * success
	    		 */
	    		if (HandlerInfo.RemainingRecvBytes == 0) {
					Status = XST_SUCCESS;
				}
	    		break;
			}
	    }

	    //Check to make sure that we successfully received data
	    if(Status == XST_SUCCESS)
	    {
	    	temp = (TemperaturePtr[0] << 8) | TemperaturePtr[1];
	    	XUartLite_Send(&UartLite, (u8*)TemperaturePtr, 2);
	    	//xil_printf("Value is: %d", temp);
	    }
	}
	/*
	 * Call the TempSensorExample.
	 */
	xil_printf("BROKE OUT OF EVENT LOOP! EXITING");
	return XST_FAILURE;

}

/****************************************************************************/
/**
 *
 * This function does a minimal test on the UartLite device and driver as a
 * design example. The purpose of this function is to illustrate
 * how to use the XUartLite component.
 *
 * This function sends data and expects to receive the same data through the
 * UartLite. The user must provide a physical loopback such that data which is
 * transmitted will be received.
 *
 * This function uses interrupt driver mode of the UartLite device. The calls
 * to the UartLite driver in the handlers should only use the non-blocking
 * calls.
 *
 * @param	DeviceId is the Device ID of the UartLite Device and is the
 *		XPAR_<uartlite_instance>_DEVICE_ID value from xparameters.h.
 *
 * @return	XST_SUCCESS if successful, otherwise XST_FAILURE.
 *
 * @note
 *
 * This function contains an infinite loop such that if interrupts are not
 * working it may never return.
 *
 ****************************************************************************/
int SetupUartLite_IIC(u16 DeviceId, u16 IicDeviceId, u8 TempSensorAddress) {
	int Status;
	int Index;
	XIic_Config *ConfigPtr;	/* Pointer to configuration data */

	/*
	 * Initialize the UartLite driver so that it's ready to use.
	 */
	Status = XUartLite_Initialize(&UartLite, DeviceId);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Perform a self-test to ensure that the hardware was built correctly.
	 */
	Status = XUartLite_SelfTest(&UartLite);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	* Initialize the IIC driver so that it is ready to use.
	*/
	ConfigPtr = XIic_LookupConfig(IicDeviceId);
	if (ConfigPtr == NULL) {
		return XST_FAILURE;
	}

	Status = XIic_CfgInitialize(&Iic, ConfigPtr,
					ConfigPtr->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}


	/*
	* Setup handler to process the asynchronous events which occur,
	* the driver is only interrupt driven such that this must be
	* done prior to starting the device.
	*/
	XIic_SetRecvHandler(&Iic, (void *)&HandlerInfo, RecvHandlerIIC);
	XIic_SetStatusHandler(&Iic, (void *)&HandlerInfo,
						StatusHandlerIIC);

	/*
	 * Connect the UartLite to the interrupt subsystem such that interrupts can
	 * occur. This function is application specific.
	 */
	Status = SetupInterruptSystem(&Iic, &UartLite);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	XIic_Start(&Iic);
	XIic_SetAddress(&Iic, XII_ADDR_TO_SEND_TYPE, TempSensorAddress);

	/*
	 * Setup the handlers for the UartLite that will be called from the
	 * interrupt context when data has been sent and received, specify a
	 * pointer to the UartLite driver instance as the callback reference so
	 * that the handlers are able to access the instance data.
	 */
	XUartLite_SetSendHandler(&UartLite, SendHandlerUART, &UartLite);
	XUartLite_SetRecvHandler(&UartLite, RecvHandlerUART, &UartLite);

	/*
	 * Enable the interrupt of the UartLite so that interrupts will occur.
	 */
	XUartLite_EnableInterrupt(&UartLite);

	/*
	 * Initialize the send buffer bytes with a pattern to send and the
	 * the receive buffer bytes to zero to allow the receive data to be
	 * verified.
	 */
	for (Index = 0; Index < TEST_BUFFER_SIZE; Index++) {
		SendBuffer[Index] = 1;
		ReceiveBuffer[Index] = 0;
	}

	/*
	* Clear updated flags such that they can be polled to indicate
	* when the handler information has changed asynchronously and
	* initialize the status which will be returned to a default value
	*/
	HandlerInfo.EventStatusUpdated = FALSE;
	HandlerInfo.RecvBytesUpdated = FALSE;
	Status = XST_FAILURE;

	/*
	* Attempt to receive a byte of data from the temperature sensor
	* on the IIC interface, ignore the return value since this example is
	* a single master system such that the IIC bus should not ever be busy
	*/
	u8 TemperaturePtr[2];
	(void)XIic_MasterRecv(&Iic, TemperaturePtr, 2);

	/*
	* The message is being received from the temperature sensor,
	* wait for it to complete by polling the information that is
	* updated asynchronously by interrupt processing
	*/
	while(1) {
		if(HandlerInfo.RecvBytesUpdated == TRUE) {
			/*
			* The device information has been updated for receive
			* processing,if all bytes received (1), indicate
			* success
			*/
			if (HandlerInfo.RemainingRecvBytes == 0) {
				Status = XST_SUCCESS;
			}
			break;
		}

		/*
		* Any event status which occurs indicates there was an error,
		* so return unsuccessful, for this example there should be no
		* status events since there is a single master on the bus
		*/
		if (HandlerInfo.EventStatusUpdated == TRUE) {
			break;
		}
	}

	return Status;
}

/****************************************************************************/
/**
 *
 * This function setups the interrupt system such that interrupts can occur
 * for the UartLite device and I2C device.
 *
 * @param    UartLitePtr contains a pointer to the instance of the UartLite
 *           component which is going to be connected to the interrupt
 *           controller.
 *
 * @return   XST_SUCCESS if successful, otherwise XST_FAILURE.
 *
 * @note     None.
 *
 ****************************************************************************/
int SetupInterruptSystem(XIic *IicPtr, XUartLite *UartLitePtr) {

	int Status;

	/*
	 * Initialize the interrupt controller driver so that it is ready to
	 * use.
	 */
	Status = XIntc_Initialize(&InterruptController, INTC_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Connect a device driver handler that will be called when an interrupt
	 * for the device occurs, the device driver handler performs the
	 * specific interrupt processing for the device.
	 */
	Status = XIntc_Connect(&InterruptController, UARTLITE_INT_IRQ_ID,
			(XInterruptHandler) XUartLite_InterruptHandler,
			(void *) UartLitePtr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
		 * Connect a device driver handler that will be called when an interrupt
		 * for the device occurs, the device driver handler performs the
		 * specific interrupt processing for the device
		 */
		Status = XIntc_Connect(&InterruptController, INTC_IIC_INTERRUPT_ID,
						XIic_InterruptHandler, IicPtr);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

	/*
	 * Start the interrupt controller such that interrupts are enabled for
	 * all devices that cause interrupts, specific real mode so that
	 * the UartLite can cause interrupts through the interrupt controller.
	 */
	Status = XIntc_Start(&InterruptController, XIN_REAL_MODE);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Enable the interrupt for the UartLite device.
	 */
	XIntc_Enable(&InterruptController, UARTLITE_INT_IRQ_ID);

	/*
	 * Enable the interrupts for the IIC device.
	*/
	XIntc_Enable(&InterruptController, INTC_IIC_INTERRUPT_ID);

	/*
	 * Initialize the exception table.
	 */
	Xil_ExceptionInit();

	/*
	 * Register the interrupt controller handler with the exception table.
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			(Xil_ExceptionHandler) XIntc_InterruptHandler,
			&InterruptController);

	/*
	 * Enable exceptions.
	 */
	Xil_ExceptionEnable();

	return XST_SUCCESS;
}



/*****************************************************************************/
/**
* This receive handler is called asynchronously from an interrupt context and
* and indicates that data in the specified buffer was received. The byte count
* should equal the byte count of the buffer if all the buffer was filled.
*
* @param	CallBackRef is a pointer to the IIC device driver instance which
*		the handler is being called for.
* @param	ByteCount indicates the number of bytes remaining to be received of
*		the requested byte count. A value of zero indicates all requested
*		bytes were received.
*
* @return	None.
*
* @notes	None.
*
****************************************************************************/
static void RecvHandlerIIC(void *CallbackRef, int ByteCount)
{
	HandlerInfo.RemainingRecvBytes = ByteCount;
	HandlerInfo.RecvBytesUpdated = TRUE;
}

/*****************************************************************************/
/**
* This status handler is called asynchronously from an interrupt context and
* indicates that the conditions of the IIC bus changed. This  handler should
* not be called for the application unless an error occurs.
*
* @param	CallBackRef is a pointer to the IIC device driver instance which the
*		handler is being called for.
* @param	Status contains the status of the IIC bus which changed.
*
* @return	None.
*
* @notes	None.
*
****************************************************************************/
static void StatusHandlerIIC(void *CallbackRef, int Status)
{
	HandlerInfo.EventStatus |= Status;
	HandlerInfo.EventStatusUpdated = TRUE;
}


/*****************************************************************************/
/**
 *
 * This function is the handler which performs processing to send data to the
 * UartLite. It is called from an interrupt context such that the amount of
 * processing performed should be minimized. It is called when the transmit
 * FIFO of the UartLite is empty and more data can be sent through the UartLite.
 *
 * This handler provides an example of how to handle data for the UartLite,
 * but is application specific.
 *
 * @param	CallBackRef contains a callback reference from the driver.
 *		In this case it is the instance pointer for the UartLite driver.
 * @param	EventData contains the number of bytes sent or received for sent
 *		and receive events.
 *
 * @return	None.
 *
 * @note		None.
 *
 ****************************************************************************/
void SendHandlerUART(void *CallBackRef, unsigned int EventData) {
	TotalSentCount = EventData;
}

/****************************************************************************/
/**
 *
 * This function is the handler which performs processing to receive data from
 * the UartLite. It is called from an interrupt context such that the amount of
 * processing performed should be minimized.  It is called data is present in
 * the receive FIFO of the UartLite such that the data can be retrieved from
 * the UartLite. The size of the data present in the FIFO is not known when
 * this function is called.
 *
 * This handler provides an example of how to handle data for the UartLite,
 * but is application specific.
 *
 * @param	CallBackRef contains a callback reference from the driver, in
 *		this case it is the instance pointer for the UartLite driver.
 * @param	EventData contains the number of bytes sent or received for sent
 *		and receive events.
 *
 * @return	None.
 *
 * @note		None.
 *
 ****************************************************************************/
void RecvHandlerUART(void *CallBackRef, unsigned int EventData) {
	XUartLite_Recv(&UartLite, ReceiveBufferPtr, 1);
	ReceiveBufferPtr++;
	TotalReceivedCount++;

	//If we've reached the end of the buffer, start over
	if (ReceiveBufferPtr >= (&ReceiveBuffer[0] + TEST_BUFFER_SIZE)) {
		//xil_printf("Resetting Receive Buffer. Please enter a new command!\n\r");
		ReceiveBufferPtr = &ReceiveBuffer[0];
		CommandPtr = &ReceiveBuffer[0];
		TotalReceivedCount = 0;
	}

}

/*****************************************************************************/
/**
 *
 * This function does a minimal test on the GPIO device configured as OUTPUT.
 *
 * @param	16 bit value read from the temperature sensor
 *
 * @return	- XST_SUCCESS if the example has completed successfully.
 *			- XST_FAILURE if the example has failed.
 *
 * @note   Should write the value of the temperature to the 7 segment display
 *
 ****************************************************************************/
static int SevenSegValue(u32 LED_Value) {

	//Change the value of the LED accordingly
	/*
	if (LED_Value == 0xFF) {
		xil_printf("GPIO LED is off, turning on!\n\r");
	} else if (LED_Value == 0x00) {
		xil_printf("GPIO LED is on, turning off!\n\r");
	} else {
		xil_printf("Programming LED with ASCII representation of : %c\n\r",
				LED_Value);
	}*/

	// Set the LED to the requested state
	XGpio_WriteReg((SEVEN_SEG_BASE_REG),
			((LED_CHANNEL - 1) * XGPIO_CHAN_OFFSET) + XGPIO_DATA_OFFSET,
			LED_Value);

	// Return
	return XST_SUCCESS;

}




