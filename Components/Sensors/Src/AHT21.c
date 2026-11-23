#include "AHT21.h"
#include "FreeRTOS.h"
#include "task.h"
#include "iic_hal.h"

#define AHT21_ADDRESS       0x38U
#define AHT21_BUSY          0x80U
#define AHT21_STARTUP_MASK  0x18U
#define AHT21_CALIBRATED       0x08U
#define AHT21_FRAME_SIZE       7U
#define AHT21_RAW_SCALE        (1UL << 20)

static iic_bus_t AHT21_config = {
    .IIC_SDA_PORT = GPIOB,
    .IIC_SCL_PORT = GPIOB,
    .IIC_SDA_PIN = GPIO_PIN_13,
    .IIC_SCL_PIN = GPIO_PIN_14,
};

volatile AHT21_Sample_t AHT21_Sample = {0};

static void AHT21_DelayMs(uint16_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static uint8_t AHT21_WriteCommand(uint8_t command,uint8_t first,uint8_t second)
{
    const uint8_t bytes[] = {
        AHT21_ADDRESS << 1,
        command,
        first,
        second
    };
    uint8_t index;

    if (!IIC_SensorBusLock())
        return 0U;

    IICStart(&AHT21_config);
    for (index = 0; index < sizeof(bytes); index++)
    {
        IICSendByte(&AHT21_config, bytes[index]);
        if (IICWaitAck(&AHT21_config) != SUCCESS)
        {
            IIC_SensorBusUnlock();
            return 0U;
        }
    }
    IICStop(&AHT21_config);
    IIC_SensorBusUnlock();
    return 1U;
}

static uint8_t AHT21_ReadBytes(uint8_t *bytes,uint8_t count)
{
    uint8_t index;

    if (!IIC_SensorBusLock())
        return 0U;

    IICStart(&AHT21_config);
    IICSendByte(&AHT21_config, (AHT21_ADDRESS << 1)|1);
    if (IICWaitAck(&AHT21_config) != SUCCESS)
    {
        IIC_SensorBusUnlock();
        return 0U;
    }
    for(index=0;index<count;index++)
    {
        bytes[index] = IICReceiveByte(&AHT21_config);
        if(index + 1U < count)
            IICSendAck(&AHT21_config);
    }
    IICSendNotAck(&AHT21_config);
    IICStop(&AHT21_config);
    IIC_SensorBusUnlock();
    return 1U;
}

static uint8_t AHT21_RestoreRegister(uint8_t address)
{
    uint8_t response[3];
    if(!AHT21_WriteCommand(address, 0, 0))
        return 0U;
    AHT21_DelayMs(5U);
    if(!AHT21_ReadBytes(response, sizeof(response)))
        return 0U;
    AHT21_DelayMs(10U);
    return AHT21_WriteCommand(0xB0U | address, response[1], response[2]);
}

uint8_t AHT21_Init(void)
{
    uint8_t status;
    uint32_t levels;
    const uint16_t pins = AHT21_config.IIC_SDA_PIN | AHT21_config.IIC_SCL_PIN;
    GPIO_InitTypeDef gpio = {0};
    AHT21_Status_t fault;

    if (!IIC_SensorBusLock())
        return 0U;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOB, pins, GPIO_PIN_SET);
	gpio.Pin = pins;
	gpio.Mode = GPIO_MODE_OUTPUT_OD;
	gpio.Pull = GPIO_NOPULL;
	gpio.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &gpio);
	IIC_SensorBusUnlock();
	AHT21_DelayMs(500U);
	if (!IIC_SensorBusLock())
		return 0U;

	levels = GPIOB->IDR & pins;
	IIC_SensorBusUnlock();
	if (levels != pins)
	{
		if (levels == 0U)
			fault = AHT21_BOTH_LOW;
		else if (!(levels & AHT21_config.IIC_SCL_PIN))
			fault = AHT21_SCL_LOW;
		else
			fault = AHT21_SDA_LOW;
	}
	else if (!AHT21_ReadBytes(&status, 1U))
	{
		fault = AHT21_ADDRESS_NACK;
	}
	else if (status & AHT21_BUSY)
	{
		fault = AHT21_READ_ERROR;
	}
	else
	{
		fault = AHT21_READ_ERROR;
		if ((status & AHT21_STARTUP_MASK) == AHT21_STARTUP_MASK)
			return 1U;
		if (AHT21_RestoreRegister(0x1BU) && AHT21_RestoreRegister(0x1CU) &&
		    AHT21_RestoreRegister(0x1EU))
		{
			AHT21_DelayMs(10U);
			if (AHT21_ReadBytes(&status, 1U) &&
			    (status & (AHT21_BUSY | AHT21_CALIBRATED)) == AHT21_CALIBRATED)
				return 1U;
		}
	}

	taskENTER_CRITICAL();
	AHT21_Sample.valid = 0U;
	AHT21_Sample.status = fault;
	taskEXIT_CRITICAL();
	return 0U;
}

static uint8_t AHT21_ReadFrame(uint8_t *frame)
{
	uint8_t attempt;
	uint8_t status;

	if (!AHT21_WriteCommand(0xACU, 0x33U, 0U))
	{
		return 0U;
	}
	AHT21_DelayMs(80U);
	for (attempt = 0U; attempt < 6U; attempt++)
	{
		if (attempt != 0U)
		{
			AHT21_DelayMs(10U);
		}
		if (!AHT21_ReadBytes(&status, 1U))
		{
			return 0U;
		}
		if (!(status & AHT21_BUSY))
		{
			return AHT21_ReadBytes(frame, AHT21_FRAME_SIZE) &&
			       (frame[0] & (AHT21_BUSY | AHT21_CALIBRATED)) == AHT21_CALIBRATED;
		}
	}
	return 0U;
}

static uint8_t AHT21_Crc8(const uint8_t *bytes, uint8_t count)
{
	uint8_t crc = 0xFFU;
	uint8_t index;
	uint8_t bit;

	for (index = 0U; index < count; index++)
	{
		crc ^= bytes[index];
		for (bit = 0U; bit < 8U; bit++)
		{
			crc = (uint8_t)((crc << 1) ^ ((crc & 0x80U) ? 0x31U : 0U));
		}
	}
	return crc;
}

uint8_t AHT21_Read(void)
{
	AHT21_Sample_t sample = {0};
	uint8_t frame[AHT21_FRAME_SIZE];
	uint32_t raw_humidity;
	uint32_t raw_temperature;

	if (AHT21_ReadFrame(frame) && AHT21_Crc8(frame, AHT21_FRAME_SIZE - 1U) == frame[6])
	{
		raw_humidity = ((uint32_t)frame[1] << 12) | ((uint32_t)frame[2] << 4) | (frame[3] >> 4);
		raw_temperature = ((uint32_t)(frame[3] & 0x0FU) << 16) | ((uint32_t)frame[4] << 8) | frame[5];
		sample.humidity_x10 = (uint16_t)((raw_humidity * 1000UL + AHT21_RAW_SCALE / 2U) / AHT21_RAW_SCALE);
		sample.temperature_x10 = (int16_t)((raw_temperature * 2000UL + AHT21_RAW_SCALE / 2U) / AHT21_RAW_SCALE) - 500;
		sample.valid = 1U;
	}
	else
	{
		sample.status = AHT21_READ_ERROR;
	}

	taskENTER_CRITICAL();
	AHT21_Sample = sample;
	taskEXIT_CRITICAL();
	return sample.valid;
}

