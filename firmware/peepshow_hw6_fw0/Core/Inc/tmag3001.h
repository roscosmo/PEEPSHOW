/**
 * \brief This header file contains all register map definitions for the TMAG3001.
 *
 * \copyright Copyright (C) 2022 Texas Instruments Incorporated - http://www.ti.com/
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef TMAG3001_H_
#define TMAG3001_H_

// Standard libraries
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

// Custom libraries
#include "hal.h"


//****************************************************************************
//
// Constants
//
//****************************************************************************

#define NUM_REGISTERS                           ((uint8_t) 29)

// Length of an I2C frame for the TMAG3001 in bytes
#define TMAG3001_FRAME_NUM_BYTES                    1

#define   MAX_DELAYS_IN_OPMODE_CHANGES

//****************************************************************************
//****************************************************************************
//
// Function prototypes
//
//****************************************************************************
//****************************************************************************

void TMAG3001startup();
void TMAG3001restoreDefaultValues();
void TMAG3001updateI2Caddress(uint8_t i2c_address);
void TMAG3001intbPolarity(uint8_t intb_pol);

//***************//
// I2C Functions //
//***************//
void    TMAG3001writeToSingleRegister(uint8_t address, uint8_t data);
void    TMAG3001writeToRegisterWithTrigger(uint8_t address, uint8_t data);
void    TMAG3001writeToMultipleRegisters(uint8_t startAddress, const uint8_t count, uint8_t triggerBit, const uint8_t data[]);
uint8_t TMAG3001readSingleRegister(uint8_t address);
uint8_t TMAG3001readSingleRegisterWithTrigger(uint8_t address);
void    TMAG3001readMultipleRegisters(uint8_t startAddress, uint8_t count, uint8_t triggerBit, uint8_t data_read_results[]);
void    TMAG3001oneByteRead(uint8_t numChannels, uint8_t bitRead, uint8_t crc_en, uint8_t data_read_results[]);
void    TMAG3001setI2CreadMode(uint8_t read_mode);
void    TMAG3001enableCRC();
void    TMAG3001disableCRC();
void    TMAG3001i2cGlitchFilterEnabled();
void    TMAG3001i2cGlitchFilterDisabled();

//******************************//
// Change Device Operation Mode //
//******************************//
void TMAG3001enterStandbyMode();
void TMAG3001enterContinuousMeasureMode();
void TMAG3001enterSleepMode();
void TMAG3001enterWakeUpAndSleepMode();
void TMAG3001setSLEEPTIME(uint8_t sleeptime);
void TMAG3001setWakeUpAndSleepMode(uint8_t sleeptime);
void TMAG3001exitSleepMode();
void TMAG3001exitWakeAndSleepMode();

//*****************************************//
// Functions to Configure Trigger Settings //
//*****************************************//
void TMAG3001i2cTriggersConversion();
void TMAG3001intTriggersConversion();

//*************************//
// Interrupt Configuration //
//*************************//
void TMAG3001interruptThroughINT(uint8_t i2c_busy);
void TMAG3001interruptThroughSCL(uint8_t i2c_busy);
void TMAG3001intPinLatchedPulsed(uint8_t int_state);
void TMAG3001thrDirCheck(uint8_t dir);
void TMAG3001enableResultReadyInterrupt();
void TMAG3001disableResultReadyInterrupt();
void TMAG3001enableSwitchMode(uint8_t switch_type);
void TMAG3001disableInterrupts();
void TMAG3001enableIntbPin();
void TMAG3001disableIntbPin();
void TMAG3001enableWakeOnAngleChange(uint8_t angle_hyst);
void TMAG3001enableWakeOnFieldChange(uint8_t thr_hyst);
void TMAG3001disableWakeOnChange();
void TMAG3001setAngleThreshold(uint8_t angle_offest_en, uint8_t angle_offset_dir, uint8_t angle_ref, uint8_t angle_band, uint8_t angle_offset);
void TMAG3001setAngleHyst(uint8_t angle_hys);
void TMAG3001setThrHyst(uint8_t thr_hys);
void TMAG3001setFieldThreshold(uint8_t x_thr_lo, uint8_t x_thr_hi, uint8_t y_thr_lo, uint8_t y_thr_hi, uint8_t z_thr_lo, uint8_t z_thr_hi);
void TMAG3001setMagnitudeThreshold(uint8_t m_thr_lo, uint8_t m_thr_hi);
void TMAG3001disableThresholds();

//*************************************//
// Measurement Configuration Functions //
//*************************************//
void TMAG3001setTempCoefficient(uint8_t temp_coefficient);
void TMAG3001setSampleRate(uint8_t CONV_AVG_bits);
void TMAG3001enableMagChannels(uint8_t mag_ch_en_bits);
void TMAG3001setTemperatureAveraging(uint8_t temp_avg);
void TMAG3001enableAngleMeasurement(uint8_t angle_en_bits);
void TMAG3001setRanges(uint8_t x_y_range_bits, uint8_t z_range_bits);
void TMAG3001selectLowCurrentNoiseMode(uint8_t low_mode);

//**************************************//
// Offset and Gain Correction Functions //
//**************************************//
void TMAG3001setMagGainConfigIn8Bit(uint8_t channel, uint8_t gain_bits);
void TMAG3001setMagGainConfigInDecimal(uint8_t channel, float gain_value);
void TMAG3001setMagOffsetIn8Bit(uint8_t offset1_bits, uint8_t offset2_bits);
void TMAG3001setMagOffsetInmT(const float offset1_delta , const float offset2_delta);

//*******************************************************************//
// Get Results/Measurement Functions (Standard 3-byte I2C Read Only) //
//*******************************************************************//
uint8_t TMAG3001getXMSBresult();
uint8_t TMAG3001getXLSBresult();
uint8_t TMAG3001getYMSBresult();
uint8_t TMAG3001getYLSBresult();
uint8_t TMAG3001getZMSBresult();
uint8_t TMAG3001getZLSBresult();
uint8_t TMAG3001getTempMSBresult();
uint8_t TMAG3001getTempLSBresult();
uint8_t TMAG3001getAngleMSBresult();
uint8_t TMAG3001getAngleLSBresult();
uint8_t TMAG3001getMAGresult();
void    TMAG3001getMagResultsRegisters(int16_t meas_arr[]);
float   TMAG3001getMeasurementX();
float   TMAG3001getMeasurementY();
float   TMAG3001getMeasurementZ();
float   TMAG3001getMeasurementTEMP();
float   TMAG3001getMeasurementANGLE();
float   TMAG3001getMeasurementMAG();
void    TMAG3001getMagMeasurements(float meas_arr[]);

//***********************************************************//
// Get Results/Measurement Functions (1-byte I2C Reads Only) //
//***********************************************************//
void TMAG3001getOneByteMeasurements(const uint8_t dataChannels, const uint8_t bitRead, const uint16_t xy_range, const uint16_t z_range, float data[]);

//*********************//
// Get Range Functions //
//*********************//
uint16_t TMAG3001getXYrange();
uint16_t TMAG3001getZrange();

//***************************//
// Get Device Info Functions //
//***************************//
uint8_t TMAG3001getVersion();
uint8_t TMAG3001isCRCenabled();

//************************//
// Supplemental Functions //
//************************//
void     calcCORDIC(float CORDIC_results[], int16_t numerator, int16_t denominator, uint16_t range, int16_t iteration_length);
void     atan2CORDIC(int16_t numerator, int16_t denominator, int16_t iteration_length, int32_t results[]);
void     planeAngles(int16_t axisX, int16_t axisY, int16_t axisZ,  int32_t results[]);
void     convertToSpherical(int16_t axis1, int16_t axis2, int16_t axis3,  int32_t results[]);
void     convertToCylindrical(int16_t axis1, int16_t axis2, int16_t axis3,  int32_t results[]);
uint32_t mag3D(int16_t axis1, int16_t axis2, int16_t axis3);
float    piecewiseLinearizationRegister(int16_t knownValue[], float knownError[], uint16_t known_length, int16_t measValue, uint16_t range);
float    piecewiseLinearizationFloat(float knownValue[], float knownError[], uint16_t known_length, float measValue);
float    piecewiseLinearizationAngle(uint16_t knownAngle[], float knownError[], uint16_t known_length, uint16_t measAngle);
float    piecewiseLinearizationAngleFloat(float knownAngle[], float knownError[], uint16_t known_length, float measAngle);

//******************//
// Helper Functions //
//******************//
uint8_t  TMAG3001getAngleEnValue();
uint32_t isqrt32(uint32_t h);
float    TMAG3001resultRegisterTomT(int16_t register_bits, uint16_t range);
float    TMAG3001angleRegisterToDeg(uint16_t register_bits);
uint8_t  TMAG3001calculateCRC(uint8_t i2cRead, uint8_t numChannels, uint8_t data[]);
bool     TMAG3001verifyCRC(uint8_t i2cRead, uint8_t numChannels, uint8_t data[]);
void     TMAG3001intPulse();

//****************************************************************************
//
// Definitions for use with Functions
//
//****************************************************************************

#define SENSOR_CONFIG_2_FULL_RANGE_MASK                 ((uint8_t) 0x03)

#define NO_TRIGGER                                      ((uint8_t) 0x00)
#define TRIGGER_CONV                                    ((uint8_t) 0x01)

// These 'BITS' definitions are for use as function inputs

#define MAG_TEMPCO_None                                 ((uint8_t) 0x00)
#define MAG_TEMPCO_NdBFe                                ((uint8_t) 0x01)
#define MAG_TEMPCO_SmCo                                 ((uint8_t) 0x02)
#define MAG_TEMPCO_Ceramic                              ((uint8_t) 0x03)

#define CONV_AVG_1xAverage                              ((uint8_t) 0x00)
#define CONV_AVG_2xAverage                              ((uint8_t) 0x01)
#define CONV_AVG_4xAverage                              ((uint8_t) 0x02)
#define CONV_AVG_8xAverage                              ((uint8_t) 0x03)
#define CONV_AVG_16xAverage                             ((uint8_t) 0x04)
#define CONV_AVG_32xAverage                             ((uint8_t) 0x05)

#define THR_HYST_2LSB                                   ((uint8_t) 0x00)
#define THR_HYST_4LSB                                   ((uint8_t) 0x01)
#define THR_HYST_8LSB                                   ((uint8_t) 0x02)
#define THR_HYST_16LSB                                  ((uint8_t) 0x03)
#define THR_HYST_32LSB                                  ((uint8_t) 0x04)
#define THR_HYST_64LSB                                  ((uint8_t) 0x05)
#define THR_HYST_128LSB                                 ((uint8_t) 0x06)
#define THR_HYST_256LSB                                 ((uint8_t) 0x07)

#define ANGLE_HYST_1DEG                                 ((uint8_t) 0x00)
#define ANGLE_HYST_2DEG                                 ((uint8_t) 0x01)
#define ANGLE_HYST_4DEG                                 ((uint8_t) 0x02)
#define ANGLE_HYST_8DEG                                 ((uint8_t) 0x03)

#define MAG_CH_EN_Off                                   ((uint8_t) 0x00)
#define MAG_CH_EN_X                                     ((uint8_t) 0x01)
#define MAG_CH_EN_Y                                     ((uint8_t) 0x02)
#define MAG_CH_EN_XY                                    ((uint8_t) 0x03)
#define MAG_CH_EN_Z                                     ((uint8_t) 0x04)
#define MAG_CH_EN_ZX                                    ((uint8_t) 0x05)
#define MAG_CH_EN_YZ                                    ((uint8_t) 0x06)
#define MAG_CH_EN_XYZ                                   ((uint8_t) 0x07)
#define MAG_CH_EN_XYX                                   ((uint8_t) 0x08)
#define MAG_CH_EN_YXY                                   ((uint8_t) 0x09)
#define MAG_CH_EN_YZY                                   ((uint8_t) 0x0A)
#define MAG_CH_EN_XZX                                   ((uint8_t) 0x0B)
#define MAG_CH_EN_XYZPositiveDiagOffset                 ((uint8_t) 0x0C)
#define MAG_CH_EN_XYZNegativeDiagOffset                 ((uint8_t) 0x0D)
#define MAG_CH_EN_HallResistADCCheck                    ((uint8_t) 0x0E)
#define MAG_CH_EN_HallOffsetAFECheck                    ((uint8_t) 0x0F)

#define MAG_THR_DIR_Above                               ((uint8_t) 0x00)
#define MAG_THR_DIR_Below                               ((uint8_t) 0x01)

#define ANGLE_EN_Disabled                               ((uint8_t) 0x00)
#define ANGLE_EN_X1stY2nd                               ((uint8_t) 0x01)
#define ANGLE_EN_Y1stZ2nd                               ((uint8_t) 0x02)
#define ANGLE_EN_X1stZ2nd                               ((uint8_t) 0x03)

#define RANGE_40mTor120mT                               ((uint8_t) 0x00)
#define RANGE_80mTor240mT                               ((uint8_t) 0x01)

#define WOC_Disabled                                    ((uint8_t) 0x00)
#define WOC_Angle                                       ((uint8_t) 0x01)
#define WOC_MagneticField                               ((uint8_t) 0x02)

#define THR_None                                        ((uint8_t) 0x00)
#define THR_Angle                                       ((uint8_t) 0x01)
#define THR_BField                                      ((uint8_t) 0x02)
#define THR_Magnitude                                   ((uint8_t) 0x03)

#define INT_STATE_Latched                               ((uint8_t) 0x00)
#define INT_STATE_Pulse                                 ((uint8_t) 0x01)

#define Interrupts_Disabled                             ((uint8_t) 0x00)
#define I2C_Busy_Interrupt                              ((uint8_t) 0x01)
#define I2C_Busy_Wait                                   ((uint8_t) 0x02)
#define SCL_Busy_Interrupt                              ((uint8_t) 0x03)
#define SCL_Busy_Wait                                   ((uint8_t) 0x04)
#define Unipolar_Switch_Mode                            ((uint8_t) 0x05)
#define Omnipolar_Switch_Mode                           ((uint8_t) 0x06)

/*
 * Temperature Electrical Characteristics (ECHAR)
 *
 * Currently the 'Typical' Electrical Characteristics (ECHAR) of the device are set for
 * ECHAR_T_ADC_RES and ECHAR_T_SENS_T0. These values can differ and, if through device
 * calibration their actual values for a particular device are found, can be updated for
 * more accurate temperature measurement.
 *
 * Pg. 30 of the datasheet contains the descriptions of the Temperature Sensing Electrical
 * Characteristics for the TMAG3001. The definition names match their written counterparts.
 */

// TEMP_RESULT decimal value @ ECHAR_T_SENS_T0
#define ECHAR_T_ADC_T0                                  ((float) 17512)

// Reference Temperature for ECHAR_T_ADC_T0 (C)
#define ECHAR_T_SENS_T0                                 ((float) 25) // Typical value provided datasheet, actual can differ

// Temp sensing resolution (LSB/C)
#define ECHAR_T_ADC_RES                                 ((float) 58.2) // Typical value provided by datasheet, actual can differ

/*
 * Magnetic Sensor Offset Correction Conversion
 *
 * Pg. 32 of the datasheet contains the conversions for the LSB size for each magnetic range.
 * The data format will be the same for Offset_Config_1[7:0] and
 * Offset_Config_2[7:0]. The LSB size for each magnetic range is:
 *      - 40 mT:   19.53125 uT/LSB
 *      - 80 mT:   39.06250 uT/LSB
 *      - 120 mT:  64.94139 uT/LSB
 *      - 240 mT: 129.87013 uT/LSB
 *
 */

#define VER_1_RANGE_0_OFFSET                            ((float)  0.01953125) // mT/LSB
#define VER_1_RANGE_1_OFFSET                            ((float)  0.03906250) // mT/LSB
#define VER_2_RANGE_0_OFFSET                            ((float)  0.06494139) // mT/LSB
#define VER_2_RANGE_1_OFFSET                            ((float)  0.12987013) // mT/LSB

/*
 * Magnetic Sensor Data Conversion
 *
 * Pg. 31 of the datasheet contains the conversions for the LSB size for each magnetic range.
 * The data format will be the same for X_Result[15:0], Y_Result[15:0] and Z_Result[15:0].
 * The LSB size for each magnetic range is:
 *      -  40 mT: 1.19047619 uT/LSB
 *      -  80 mT: 2.38095238 uT/LSB
 *      - 120 mT: 3.66300366 uT/LSB
 *      - 240 mT: 4.14937759 uT/LSB
 *
 */

#define VER_1_RANGE_0_DATA                              ((float)  0.00119047619) // mT/LSB
#define VER_1_RANGE_1_DATA                              ((float)  0.00238095238) // mT/LSB
#define VER_2_RANGE_0_DATA                              ((float)  0.00366300366) // mT/LSB
#define VER_2_RANGE_1_DATA                              ((float)  0.00414937759) // mT/LSB

/*
 * Magnitude Data Conversion
 *
 * Pg. 34 of the datasheet contains the conversions for the LSB size for each magnetic range.
 * The LSB size for each magnetic range is:
 *      - 40 mT:  0.30478513 mT/LSB
 *      - 80 mT:  0.60975610 mT/LSB
 *      - 120 mT: 0.93808630 mT/LSB
 *      - 240 mT: 1.86915888 mT/LSB
 *
 */

#define VER_1_RANGE_0_MAGNITUDE                         ((float)  0.30478513) // mT/LSB
#define VER_1_RANGE_1_MAGNITUDE                         ((float)  0.60975610) // mT/LSB
#define VER_2_RANGE_0_MAGNITUDE                         ((float)  0.93808630) // mT/LSB
#define VER_2_RANGE_1_MAGNITUDE                         ((float)  1.86915888) // mT/LSB

//**********************************************************************************
//
// Register definitions
//
//**********************************************************************************


/* Register 0x00 (DEVICE_CONFIG_1) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         CRC_EN        |                MAG_TEMPCO[1:0]                |                             CONV_AVG[2:0]                             |                  I2C_RD[1:0]                  |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* DEVICE_CONFIG_1 register address */
    #define DEVICE_CONFIG_1_ADDRESS                                         ((uint8_t) 0x00)

    /* DEVICE_CONFIG_1 default (reset) value */
    #define DEVICE_CONFIG_1_DEFAULT                                         ((uint8_t) 0x00)

    /* DEVICE_CONFIG_1 register field masks */
    #define DEVICE_CONFIG_1_CRC_EN_MASK                                     ((uint8_t) 0x80)
    #define DEVICE_CONFIG_1_MAG_TEMPCO_MASK                                 ((uint8_t) 0x60)
    #define DEVICE_CONFIG_1_CONV_AVG_MASK                                   ((uint8_t) 0x1C)
    #define DEVICE_CONFIG_1_I2C_RD_MASK                                     ((uint8_t) 0x03)

    /* CRC_EN field values */
    #define DEVICE_CONFIG_1_CRC_EN_Disabled                                 ((uint8_t) 0x00)
    #define DEVICE_CONFIG_1_CRC_EN_Enabled                                  ((uint8_t) 0x80)

    /* MAG_TEMPCO field values */
    #define DEVICE_CONFIG_1_MAG_TEMPCO_None                                 ((uint8_t) 0x00)
    #define DEVICE_CONFIG_1_MAG_TEMPCO_NdBFe                                ((uint8_t) 0x20)
    #define DEVICE_CONFIG_1_MAG_TEMPCO_SmCo                                 ((uint8_t) 0x40)
    #define DEVICE_CONFIG_1_MAG_TEMPCO_Ceramic                              ((uint8_t) 0x60)

    /* CONV_AVG field values */
    #define DEVICE_CONFIG_1_CONV_AVG_1xAverage                              ((uint8_t) 0x00)
    #define DEVICE_CONFIG_1_CONV_AVG_2xAverage                              ((uint8_t) 0x04)
    #define DEVICE_CONFIG_1_CONV_AVG_4xAverage                              ((uint8_t) 0x08)
    #define DEVICE_CONFIG_1_CONV_AVG_8xAverage                              ((uint8_t) 0x0C)
    #define DEVICE_CONFIG_1_CONV_AVG_16xAverage                             ((uint8_t) 0x10)
    #define DEVICE_CONFIG_1_CONV_AVG_32xAverage                             ((uint8_t) 0x14)

    /* I2C_RD field values */
    #define DEVICE_CONFIG_1_I2C_RD_StandardI2C                              ((uint8_t) 0x00)
    #define DEVICE_CONFIG_1_I2C_RD_16BitSensorData                          ((uint8_t) 0x01)
    #define DEVICE_CONFIG_1_I2C_RD_8BitSensorData                           ((uint8_t) 0x02)
    #define DEVICE_CONFIG_1_I2C_RD_Reserved                                 ((uint8_t) 0x03)



/* Register 0x01 (DEVICE_CONFIG_2) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                             THR_HYST[2:0]                             |         LP_LN         |   I2C_GLITCH_FILTER   |      TRIGGER_MODE     |              OPERATING_MODE[1:0]              |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* DEVICE_CONFIG_2 register address */
    #define DEVICE_CONFIG_2_ADDRESS                                         ((uint8_t) 0x01)

    /* DEVICE_CONFIG_2 default (reset) value */
    #define DEVICE_CONFIG_2_DEFAULT                                         ((uint8_t) 0x00)

    /* DEVICE_CONFIG_2 register field masks */
    #define DEVICE_CONFIG_2_THR_HYST_MASK                                   ((uint8_t) 0xE0)
    #define DEVICE_CONFIG_2_LP_LN_MASK                                      ((uint8_t) 0x10)
    #define DEVICE_CONFIG_2_I2C_GLITCH_FILTER_MASK                          ((uint8_t) 0x08)
    #define DEVICE_CONFIG_2_TRIGGER_MODE_MASK                               ((uint8_t) 0x04)
    #define DEVICE_CONFIG_2_OPERATING_MODE_MASK                             ((uint8_t) 0x03)

    /* THR_HYST field values */
    #define DEVICE_CONFIG_2_THR_HYST_2LSB                                   ((uint8_t) 0x00)
    #define DEVICE_CONFIG_2_THR_HYST_4LSB                                   ((uint8_t) 0x20)
    #define DEVICE_CONFIG_2_THR_HYST_8LSB                                   ((uint8_t) 0x40)
    #define DEVICE_CONFIG_2_THR_HYST_16LSB                                  ((uint8_t) 0x60)
    #define DEVICE_CONFIG_2_THR_HYST_32LSB                                  ((uint8_t) 0x80)
    #define DEVICE_CONFIG_2_THR_HYST_64LSB                                  ((uint8_t) 0xA0)
    #define DEVICE_CONFIG_2_THR_HYST_128LSB                                 ((uint8_t) 0xC0)
    #define DEVICE_CONFIG_2_THR_HYST_256LSB                                 ((uint8_t) 0xE0)

    /* LP_LN field values */
    #define DEVICE_CONFIG_2_LP_LN_LowCurrent                                ((uint8_t) 0x00)
    #define DEVICE_CONFIG_2_LP_LN_LowNoise                                  ((uint8_t) 0x10)

    /* I2C_GLITCH_FILTER field values */
    #define DEVICE_CONFIG_2_I2C_GLITCH_FILTER_On                            ((uint8_t) 0x00)
    #define DEVICE_CONFIG_2_I2C_GLITCH_FILTER_Off                           ((uint8_t) 0x08)

    /* TRIGGER_MODE field values */
    #define DEVICE_CONFIG_2_TRIGGER_MODE_I2CCommand                         ((uint8_t) 0x00)
    #define DEVICE_CONFIG_2_TRIGGER_MODE_INTpin                             ((uint8_t) 0x04)

    /* OPERATING_MODE field values */
    #define DEVICE_CONFIG_2_OPERATING_MODE_Standby                          ((uint8_t) 0x00)
    #define DEVICE_CONFIG_2_OPERATING_MODE_Sleep                            ((uint8_t) 0x01)
    #define DEVICE_CONFIG_2_OPERATING_MODE_Continuous                       ((uint8_t) 0x02)
    #define DEVICE_CONFIG_2_OPERATING_MODE_WakeupandSleep                   ((uint8_t) 0x03)



/* Register 0x02 (SENSOR_CONFIG_1) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                         MAG_CH_EN[3:0]                                        |                                         SLEEPTIME[3:0]                                        |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* SENSOR_CONFIG_1 register address */
    #define SENSOR_CONFIG_1_ADDRESS                                         ((uint8_t) 0x02)

    /* SENSOR_CONFIG_1 default (reset) value */
    #define SENSOR_CONFIG_1_DEFAULT                                         ((uint8_t) 0x00)

    /* SENSOR_CONFIG_1 register field masks */
    #define SENSOR_CONFIG_1_MAG_CH_EN_MASK                                  ((uint8_t) 0xF0)
    #define SENSOR_CONFIG_1_SLEEPTIME_MASK                                  ((uint8_t) 0x0F)

    /* MAG_CH_EN field values */
    #define SENSOR_CONFIG_1_MAG_CH_EN_Off                                   ((uint8_t) 0x00)
    #define SENSOR_CONFIG_1_MAG_CH_EN_X                                     ((uint8_t) 0x10)
    #define SENSOR_CONFIG_1_MAG_CH_EN_Y                                     ((uint8_t) 0x20)
    #define SENSOR_CONFIG_1_MAG_CH_EN_XY                                    ((uint8_t) 0x30)
    #define SENSOR_CONFIG_1_MAG_CH_EN_Z                                     ((uint8_t) 0x40)
    #define SENSOR_CONFIG_1_MAG_CH_EN_ZX                                    ((uint8_t) 0x50)
    #define SENSOR_CONFIG_1_MAG_CH_EN_YZ                                    ((uint8_t) 0x60)
    #define SENSOR_CONFIG_1_MAG_CH_EN_XYZ                                   ((uint8_t) 0x70)
    #define SENSOR_CONFIG_1_MAG_CH_EN_XYX                                   ((uint8_t) 0x80)
    #define SENSOR_CONFIG_1_MAG_CH_EN_YXY                                   ((uint8_t) 0x90)
    #define SENSOR_CONFIG_1_MAG_CH_EN_YZY                                   ((uint8_t) 0xA0)
    #define SENSOR_CONFIG_1_MAG_CH_EN_XZX                                   ((uint8_t) 0xB0)
    #define SENSOR_CONFIG_1_MAG_CH_EN_XYZPositiveDiagOffset                 ((uint8_t) 0xC0)
    #define SENSOR_CONFIG_1_MAG_CH_EN_XYZNegativeDiagOffset                 ((uint8_t) 0xD0)
    #define SENSOR_CONFIG_1_MAG_CH_EN_HallResistADCcheck                    ((uint8_t) 0xE0)
    #define SENSOR_CONFIG_1_MAG_CH_EN_HallOffsetAFEcheck                    ((uint8_t) 0xF0)

    /* SLEEPTIME field values */
    #define SENSOR_CONFIG_1_SLEEPTIME_1ms                                   ((uint8_t) 0x00)
    #define SENSOR_CONFIG_1_SLEEPTIME_5ms                                   ((uint8_t) 0x01)
    #define SENSOR_CONFIG_1_SLEEPTIME_10ms                                  ((uint8_t) 0x02)
    #define SENSOR_CONFIG_1_SLEEPTIME_15ms                                  ((uint8_t) 0x03)
    #define SENSOR_CONFIG_1_SLEEPTIME_20ms                                  ((uint8_t) 0x04)
    #define SENSOR_CONFIG_1_SLEEPTIME_30ms                                  ((uint8_t) 0x05)
    #define SENSOR_CONFIG_1_SLEEPTIME_50ms                                  ((uint8_t) 0x06)
    #define SENSOR_CONFIG_1_SLEEPTIME_100ms                                 ((uint8_t) 0x07)
    #define SENSOR_CONFIG_1_SLEEPTIME_500ms                                 ((uint8_t) 0x08)
    #define SENSOR_CONFIG_1_SLEEPTIME_1000ms                                ((uint8_t) 0x09)
    #define SENSOR_CONFIG_1_SLEEPTIME_2000ms                                ((uint8_t) 0x0A)
    #define SENSOR_CONFIG_1_SLEEPTIME_5000ms                                ((uint8_t) 0x0B)
    #define SENSOR_CONFIG_1_SLEEPTIME_20000ms                               ((uint8_t) 0x0C)



/* Register 0x03 (SENSOR_CONFIG_2) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         T_RATE        |        INTB_POL       |      MAG_THR_DIR      |      MAG_GAIN_CH      |                 ANGLE_EN[1:0]                 |       X_Y_RANGE       |        Z_RANGE        |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* SENSOR_CONFIG_2 register address */
    #define SENSOR_CONFIG_2_ADDRESS                                         ((uint8_t) 0x03)

    /* SENSOR_CONFIG_2 default (reset) value */
    #define SENSOR_CONFIG_2_DEFAULT                                         ((uint8_t) 0x00)

    /* SENSOR_CONFIG_2 register field masks */
    #define SENSOR_CONFIG_2_T_RATE_MASK                                     ((uint8_t) 0x80)
    #define SENSOR_CONFIG_2_INTB_POL_MASK                                   ((uint8_t) 0x40)
    #define SENSOR_CONFIG_2_MAG_THR_DIR_MASK                                ((uint8_t) 0x20)
    #define SENSOR_CONFIG_2_MAG_GAIN_CH_MASK                                ((uint8_t) 0x10)
    #define SENSOR_CONFIG_2_ANGLE_EN_MASK                                   ((uint8_t) 0x0C)
    #define SENSOR_CONFIG_2_X_Y_RANGE_MASK                                  ((uint8_t) 0x02)
    #define SENSOR_CONFIG_2_Z_RANGE_MASK                                    ((uint8_t) 0x01)

    /* T_RATE field values */
    #define SENSOR_CONFIG_2_T_RATE_SingleTempConv                           ((uint8_t) 0x00)
    #define SENSOR_CONFIG_2_T_RATE_FilterPerConv                            ((uint8_t) 0x80)

    /* INTB_POL field values */
    #define SENSOR_CONFIG_2_INTB_POL_DefaultHigh                            ((uint8_t) 0x00)
    #define SENSOR_CONFIG_2_INTB_POL_DefaultLow                             ((uint8_t) 0x40)

    /* MAG_THR_DIR field values */
    #define SENSOR_CONFIG_2_MAG_THR_DIR_Above                               ((uint8_t) 0x00)
    #define SENSOR_CONFIG_2_MAG_THR_DIR_Below                               ((uint8_t) 0x20)

    /* MAG_GAIN_CH field values */
    #define SENSOR_CONFIG_2_MAG_GAIN_CH_1stChannel                          ((uint8_t) 0x00)
    #define SENSOR_CONFIG_2_MAG_GAIN_CH_2ndChannel                          ((uint8_t) 0x10)

    /* ANGLE_EN field values */
    #define SENSOR_CONFIG_2_ANGLE_EN_Disabled                               ((uint8_t) 0x00)
    #define SENSOR_CONFIG_2_ANGLE_EN_X1stY2nd                               ((uint8_t) 0x04)
    #define SENSOR_CONFIG_2_ANGLE_EN_Y1stZ2nd                               ((uint8_t) 0x08)
    #define SENSOR_CONFIG_2_ANGLE_EN_X1stZ2nd                               ((uint8_t) 0x0C)

    /* X_Y_RANGE field values */
    #define SENSOR_CONFIG_2_X_Y_RANGE_DEFAULT                               ((uint8_t) 0x00)
    #define SENSOR_CONFIG_2_X_Y_RANGE_80mTor240mT                           ((uint8_t) 0x02)

    /* Z_RANGE field values */
    #define SENSOR_CONFIG_2_Z_RANGE_DEFAULT                                 ((uint8_t) 0x00)
    #define SENSOR_CONFIG_2_Z_RANGE_80mTor240mT                             ((uint8_t) 0x01)



/* Register 0x04 (THR_CONFIG_1) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                                         THRESHOLD1[7:0]                                                                                       |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* THR_CONFIG_1 register address */
    #define THR_CONFIG_1_ADDRESS                                            ((uint8_t) 0x04)

    /* THR_CONFIG_1 default (reset) value */
    #define THR_CONFIG_1_DEFAULT                                            ((uint8_t) 0x00)

    /* THR_CONFIG_1 register field masks */
    #define THR_CONFIG_1_THRESHOLD1_MASK                                    ((uint8_t) 0xFF)



/* Register 0x05 (THR_CONFIG_2) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                                         THRESHOLD2[7:0]                                                                                       |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* THR_CONFIG_2 register address */
    #define THR_CONFIG_2_ADDRESS                                            ((uint8_t) 0x05)

    /* THR_CONFIG_2 default (reset) value */
    #define THR_CONFIG_2_DEFAULT                                            ((uint8_t) 0x00)

    /* THR_CONFIG_2 register field masks */
    #define THR_CONFIG_2_THRESHOLD2_MASK                                    ((uint8_t) 0xFF)



/* Register 0x06 (THR_CONFIG_3) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                                         THRESHOLD3[7:0]                                                                                       |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* THR_CONFIG_3 register address */
    #define THR_CONFIG_3_ADDRESS                                            ((uint8_t) 0x06)

    /* THR_CONFIG_3 default (reset) value */
    #define THR_CONFIG_3_DEFAULT                                            ((uint8_t) 0x00)

    /* THR_CONFIG_3 register field masks */
    #define THR_CONFIG_3_THRESHOLD3_MASK                                    ((uint8_t) 0xFF)



/* Register 0x07 (SENSOR_CONFIG_3) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                  WOC_SEL[1:0]                 |                  THR_SEL[1:0]                 |                  ANG_HYS[1:0]                 |     ANG_OFFSET_EN     |     ANG_OFFSET_DIR    |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* SENSOR_CONFIG_3 register address */
    #define SENSOR_CONFIG_3_ADDRESS                                         ((uint8_t) 0x07)

    /* SENSOR_CONFIG_3 default (reset) value */
    #define SENSOR_CONFIG_3_DEFAULT                                         ((uint8_t) 0x00)

    /* SENSOR_CONFIG_3 register field masks */
    #define SENSOR_CONFIG_3_WOC_SEL_MASK                                    ((uint8_t) 0xC0)
    #define SENSOR_CONFIG_3_THR_SEL_MASK                                    ((uint8_t) 0x30)
    #define SENSOR_CONFIG_3_ANG_HYS_MASK                                    ((uint8_t) 0x0C)
    #define SENSOR_CONFIG_3_ANG_OFFSET_EN_MASK                              ((uint8_t) 0x02)
    #define SENSOR_CONFIG_3_ANG_OFFSET_DIR_MASK                             ((uint8_t) 0x01)

    /* WOC_SEL field values */
    #define SENSOR_CONFIG_3_WOC_SEL_Disabled                                ((uint8_t) 0x00)
    #define SENSOR_CONFIG_3_WOC_SEL_Angle                                   ((uint8_t) 0x40)
    #define SENSOR_CONFIG_3_WOC_SEL_MagneticField                           ((uint8_t) 0x80)
    #define SENSOR_CONFIG_3_WOC_SEL_Reserved                                ((uint8_t) 0xC0)

    /* THR_SEL field values */
    #define SENSOR_CONFIG_3_THR_SEL_None                                    ((uint8_t) 0x00)
    #define SENSOR_CONFIG_3_THR_SEL_Angle                                   ((uint8_t) 0x10)
    #define SENSOR_CONFIG_3_THR_SEL_BField                                  ((uint8_t) 0x20)
    #define SENSOR_CONFIG_3_THR_SEL_Magnitude                               ((uint8_t) 0x30)

    /* ANG_HYS field values */
    #define SENSOR_CONFIG_3_ANG_HYS_1deg                                    ((uint8_t) 0x00)
    #define SENSOR_CONFIG_3_ANG_HYS_2deg                                    ((uint8_t) 0x04)
    #define SENSOR_CONFIG_3_ANG_HYS_4deg                                    ((uint8_t) 0x08)
    #define SENSOR_CONFIG_3_ANG_HYS_8deg                                    ((uint8_t) 0x0C)

    /* ANG_OFFSET_EN field values */
    #define SENSOR_CONFIG_3_ANG_OFFSET_EN_None                              ((uint8_t) 0x00)
    #define SENSOR_CONFIG_3_ANG_OFFSET_EN_Add                               ((uint8_t) 0x02)

    /* ANG_OFFSET_DIR field values */
    #define SENSOR_CONFIG_3_ANG_OFFSET_DIR_Add                              ((uint8_t) 0x00)
    #define SENSOR_CONFIG_3_ANG_OFFSET_DIR_Subtract                         ((uint8_t) 0x01)



/* Register 0x08 (INT_CONFIG_1) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |        RSLT_INT       |       THRSLD_INT      |       INT_STATE       |                             INT_MODE[2:0]                             |        Reserved       |       MASK_INTB       |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* INT_CONFIG_1 register address */
    #define INT_CONFIG_1_ADDRESS                                            ((uint8_t) 0x08)

    /* INT_CONFIG_1 default (reset) value */
    #define INT_CONFIG_1_DEFAULT                                            ((uint8_t) 0x00)

    /* INT_CONFIG_1 register field masks */
    #define INT_CONFIG_1_RSLT_INT_MASK                                      ((uint8_t) 0x80)
    #define INT_CONFIG_1_THRSLD_INT_MASK                                    ((uint8_t) 0x40)
    #define INT_CONFIG_1_INT_STATE_MASK                                     ((uint8_t) 0x20)
    #define INT_CONFIG_1_INT_MODE_MASK                                      ((uint8_t) 0x1C)
    #define INT_CONFIG_1_INTB_POL_EN_MASK                                   ((uint8_t) 0x02)
    #define INT_CONFIG_1_MASK_INTB_MASK                                     ((uint8_t) 0x01)

    /* RSLT_INT field values */
    #define INT_CONFIG_1_RSLT_INT_Disabled                                  ((uint8_t) 0x00)
    #define INT_CONFIG_1_RSLT_INT_Enabled                                   ((uint8_t) 0x80)

    /* THRSLD_INT field values */
    #define INT_CONFIG_1_THRSLD_INT_Disabled                                ((uint8_t) 0x00)
    #define INT_CONFIG_1_THRSLD_INT_Enabled                                 ((uint8_t) 0x40)

    /* INT_STATE field values */
    #define INT_CONFIG_1_INT_STATE_Latch                                    ((uint8_t) 0x00)
    #define INT_CONFIG_1_INT_STATE_Pulse                                    ((uint8_t) 0x20)

    /* INT_MODE field values */
    #define INT_CONFIG_1_INT_MODE_None                                      ((uint8_t) 0x00)
    #define INT_CONFIG_1_INT_MODE_INT                                       ((uint8_t) 0x04)
    #define INT_CONFIG_1_INT_MODE_INTIgnoreIfI2C                            ((uint8_t) 0x08)
    #define INT_CONFIG_1_INT_MODE_SCL                                       ((uint8_t) 0x0C)
    #define INT_CONFIG_1_INT_MODE_SCLIgnoreIfI2C                            ((uint8_t) 0x10)
    #define INT_CONFIG_1_INT_MODE_Unipolar                                  ((uint8_t) 0x14)
    #define INT_CONFIG_1_INT_MODE_Omnipolar                                 ((uint8_t) 0x18)
    #define INT_CONFIG_1_INT_MODE_Invalid                                   ((uint8_t) 0x1C)

    /* MASK_INTB field values */
    #define INT_CONFIG_1_MASK_INTB_Enabled                                  ((uint8_t) 0x00)
    #define INT_CONFIG_1_MASK_INTB_Disabled                                 ((uint8_t) 0x01)



/* Register 0x09 (SENSOR_CONFIG_4) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                                        GAIN_XTHRHI[7:0]                                                                                       |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* SENSOR_CONFIG_4 register address */
    #define SENSOR_CONFIG_4_ADDRESS                                         ((uint8_t) 0x09)

    /* SENSOR_CONFIG_4 default (reset) value */
    #define SENSOR_CONFIG_4_DEFAULT                                         ((uint8_t) 0x00)

    /* SENSOR_CONFIG_4 register field masks */
    #define SENSOR_CONFIG_4_GAIN_XTHRHI_MASK                                ((uint8_t) 0xFF)



/* Register 0x0A (SENSOR_CONFIG_5) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                                      OFFSET1_YTHRHI[7:0]                                                                                      |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* SENSOR_CONFIG_5 register address */
    #define SENSOR_CONFIG_5_ADDRESS                                         ((uint8_t) 0x0A)

    /* SENSOR_CONFIG_5 default (reset) value */
    #define SENSOR_CONFIG_5_DEFAULT                                         ((uint8_t) 0x00)

    /* SENSOR_CONFIG_5 register field masks */
    #define SENSOR_CONFIG_5_OFFSET1_YTHRHI_MASK                             ((uint8_t) 0xFF)



/* Register 0x0B (SENSOR_CONFIG_6) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                                      OFFSET2_ZTHRHI[7:0]                                                                                      |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* SENSOR_CONFIG_6 register address */
    #define SENSOR_CONFIG_6_ADDRESS                                         ((uint8_t) 0x0B)

    /* SENSOR_CONFIG_6 default (reset) value */
    #define SENSOR_CONFIG_6_DEFAULT                                         ((uint8_t) 0x00)

    /* SENSOR_CONFIG_6 register field masks */
    #define SENSOR_CONFIG_6_OFFSET2_ZTHRHI_MASK                             ((uint8_t) 0xFF)



/* Register 0x0C (I2C_ADDRESS) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                            I2C_ADDRESS[6:0]                                                                           | I2C_ADDRESS_UPDATE_EN |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* I2C_ADDRESS register address */
    #define I2C_ADDRESS_ADDRESS                                             ((uint8_t) 0x0C)

    /* I2C_ADDRESS default (reset) value */
    #define I2C_ADDRESS_DEFAULT                                             ((uint8_t) 0x00)

    /* I2C_ADDRESS register field masks */
    #define I2C_ADDRESS_I2C_ADDRESS_MASK                                    ((uint8_t) 0xFE)
    #define I2C_ADDRESS_I2C_ADDRESS_UPDATE_EN_MASK                          ((uint8_t) 0x01)

    /* I2C_ADDRESS_UPDATE_EN field values */
    #define I2C_ADDRESS_I2C_ADDRESS_UPDATE_EN_Disabled                      ((uint8_t) 0x00)
    #define I2C_ADDRESS_I2C_ADDRESS_UPDATE_EN_Enabled                       ((uint8_t) 0x01)



/* Register 0x0D (DEVICE_ID) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |           0           |           0           |           0           |           0           |                    VER[1:0]                   |           0           |           0           |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* DEVICE_ID register address */
    #define DEVICE_ID_ADDRESS                                               ((uint8_t) 0x0D)

    /* DEVICE_ID default (reset) value */
    #define DEVICE_ID_DEFAULT                                               ((uint8_t) 0x00)

    /* DEVICE_ID register field masks */
    #define DEVICE_ID_VER_MASK                                              ((uint8_t) 0x08)

    /* VER field values */
    #define DEVICE_ID_VER_TMAG3001A1                                        ((uint8_t) 0x00)
    #define DEVICE_ID_VER_TMAG3001A2                                        ((uint8_t) 0x08)



/* Register 0x0E (MANUFACTURER_ID_LSB) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                                      MANUFACTURER_ID[7:0]                                                                                     |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* MANUFACTURER_ID_LSB register address */
    #define MANUFACTURER_ID_LSB_ADDRESS                                     ((uint8_t) 0x0E)

    /* MANUFACTURER_ID_LSB default (reset) value */
    #define MANUFACTURER_ID_LSB_DEFAULT                                     ((uint8_t) 0x49)

    /* MANUFACTURER_ID_LSB register field masks */
    #define MANUFACTURER_ID_LSB_MANUFACTURER_ID_MASK                        ((uint8_t) 0xFF)



/* Register 0x0F (MANUFACTURER_ID_MSB) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                                     MANUFACTURER_ID[15:8]                                                                                     |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* MANUFACTURER_ID_MSB register address */
    #define MANUFACTURER_ID_MSB_ADDRESS                                     ((uint8_t) 0x0F)

    /* MANUFACTURER_ID_MSB default (reset) value */
    #define MANUFACTURER_ID_MSB_DEFAULT                                     ((uint8_t) 0x54)

    /* MANUFACTURER_ID_MSB register field masks */
    #define MANUFACTURER_ID_MSB_MANUFACTURER_ID_MASK                        ((uint8_t) 0xFF)



/* Register 0x10 (TEMP_RESULT_MSB) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                                       TEMP_RESULT[15:8]                                                                                       |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* TEMP_RESULT_MSB register address */
    #define TEMP_RESULT_MSB_ADDRESS                                         ((uint8_t) 0x10)

    /* TEMP_RESULT_MSB default (reset) value */
    #define TEMP_RESULT_MSB_DEFAULT                                         ((uint8_t) 0x00)

    /* TEMP_RESULT_MSB register field masks */
    #define TEMP_RESULT_MSB_TEMP_RESULT_MASK                                ((uint8_t) 0xFF)



/* Register 0x11 (TEMP_RESULT_LSB) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                                       TEMP_RESULT[7:0]                                                                                        |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* TEMP_RESULT_LSB register address */
    #define TEMP_RESULT_LSB_ADDRESS                                         ((uint8_t) 0x11)

    /* TEMP_RESULT_LSB default (reset) value */
    #define TEMP_RESULT_LSB_DEFAULT                                         ((uint8_t) 0x00)

    /* TEMP_RESULT_LSB register field masks */
    #define TEMP_RESULT_LSB_TEMP_RESULT_MASK                                ((uint8_t) 0xFF)



/* Register 0x12 (X_RESULT_MSB) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                                          X_RESULT[15:8]                                                                                       |                                                                                     X_CH_RESULT [7:0][7:0]                                                                                    |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* X_RESULT_MSB register address */
    #define X_RESULT_MSB_ADDRESS                                            ((uint8_t) 0x12)

    /* X_RESULT_MSB default (reset) value */
    #define X_RESULT_MSB_DEFAULT                                            ((uint8_t) 0x00)

    /* X_RESULT_MSB register field masks */
    #define X_RESULT_MSB_X_RESULT_MASK                                      ((uint8_t) 0xFF)



/* Register 0x13 (X_RESULT_LSB) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                                         X_RESULT[7:0]                                                                                         |                                                                                     X_CH_RESULT [7:0][7:0]                                                                                    |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* X_RESULT_LSB register address */
    #define X_RESULT_LSB_ADDRESS                                            ((uint8_t) 0x13)

    /* X_RESULT_LSB default (reset) value */
    #define X_RESULT_LSB_DEFAULT                                            ((uint8_t) 0x00)

    /* X_RESULT_LSB register field masks */
    #define X_RESULT_LSB_X_RESULT_MASK                                      ((uint8_t) 0xFF)



/* Register 0x14 (Y_RESULT_MSB) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                                         Y_RESULT[15:8]                                                                                        |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* Y_RESULT_MSB register address */
    #define Y_RESULT_MSB_ADDRESS                                            ((uint8_t) 0x14)

    /* Y_RESULT_MSB default (reset) value */
    #define Y_RESULT_MSB_DEFAULT                                            ((uint8_t) 0x00)

    /* Y_RESULT_MSB register field masks */
    #define Y_RESULT_MSB_Y_RESULT_MASK                                      ((uint8_t) 0xFF)



/* Register 0x15 (Y_RESULT_LSB) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                                          Y_RESULT[7:0]                                                                                        |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* Y_RESULT_LSB register address */
    #define Y_RESULT_LSB_ADDRESS                                            ((uint8_t) 0x15)

    /* Y_RESULT_LSB default (reset) value */
    #define Y_RESULT_LSB_DEFAULT                                            ((uint8_t) 0x00)

    /* Y_RESULT_LSB register field masks */
    #define Y_RESULT_LSB_Y_RESULT_MASK                                      ((uint8_t) 0xFF)



/* Register 0x16 (Z_RESULT_MSB) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                                         Z_RESULT[15:8]                                                                                        |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* Z_RESULT_MSB register address */
    #define Z_RESULT_MSB_ADDRESS                                            ((uint8_t) 0x16)

    /* Z_RESULT_MSB default (reset) value */
    #define Z_RESULT_MSB_DEFAULT                                            ((uint8_t) 0x00)

    /* Z_RESULT_MSB register field masks */
    #define Z_RESULT_MSB_Z_RESULT_MASK                                      ((uint8_t) 0xFF)



/* Register 0x17 (Z_RESULT_LSB) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                                         Z_RESULT[7:0]                                                                                         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* Z_RESULT_LSB register address */
    #define Z_RESULT_LSB_ADDRESS                                            ((uint8_t) 0x17)

    /* Z_RESULT_LSB default (reset) value */
    #define Z_RESULT_LSB_DEFAULT                                            ((uint8_t) 0x00)

    /* Z_RESULT_LSB register field masks */
    #define Z_RESULT_LSB_Z_RESULT_MASK                                      ((uint8_t) 0xFF)



/* Register 0x18 (CONV_STATUS) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                             SET_COUNT[2:0]                            |          POR          |           0           |           0           |      DIAG_STATUS      |     RESULT_STATUS     |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* CONV_STATUS register address */
    #define CONV_STATUS_ADDRESS                                             ((uint8_t) 0x18)

    /* CONV_STATUS default (reset) value */
    #define CONV_STATUS_DEFAULT                                             ((uint8_t) 0x10)

    /* CONV_STATUS register field masks */
    #define CONV_STATUS_SET_COUNT_MASK                                      ((uint8_t) 0xE0)
    #define CONV_STATUS_POR_MASK                                            ((uint8_t) 0x10)
    #define CONV_STATUS_DIAG_STATUS_MASK                                    ((uint8_t) 0x02)
    #define CONV_STATUS_RESULT_STATUS_MASK                                  ((uint8_t) 0x01)

    /* POR field values */
    #define CONV_STATUS_POR_False                                           ((uint8_t) 0x00)
    #define CONV_STATUS_POR_True                                            ((uint8_t) 0x10)

    /* DIAG_STATUS field values */
    #define CONV_STATUS_DIAG_STATUS_NoFail                                  ((uint8_t) 0x00)
    #define CONV_STATUS_DIAG_STATUS_Fail                                    ((uint8_t) 0x02)

    /* RESULT_STATUS field values */
    #define CONV_STATUS_RESULT_STATUS_Incomplete                            ((uint8_t) 0x00)
    #define CONV_STATUS_RESULT_STATUS_Complete                              ((uint8_t) 0x01)



/* Register 0x19 (ANGLE_RESULT_MSB) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |           0           |           0           |           0           |                                                 ANGLE_RESULT_MSB[12:8]                                                |                                                                                     ANGLE_RESULT_LSB[7:0]                                                                                     |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* ANGLE_RESULT_MSB register address */
    #define ANGLE_RESULT_MSB_ADDRESS                                        ((uint8_t) 0x19)

    /* ANGLE_RESULT_MSB default (reset) value */
    #define ANGLE_RESULT_MSB_DEFAULT                                        ((uint8_t) 0x00)

    /* ANGLE_RESULT_MSB register field masks */
    #define ANGLE_RESULT_MSB_ANGLE_RESULT_MASK                              ((uint8_t) 0xFF)



/* Register 0x1A (ANGLE_RESULT_LSB) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                                    ANGLE_RESULT_MSB[7:0]                                                                                      |                                                                                     ANGLE_RESULT_LSB[7:0]                                                                                     |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* ANGLE_RESULT_LSB register address */
    #define ANGLE_RESULT_LSB_ADDRESS                                        ((uint8_t) 0x1A)

    /* ANGLE_RESULT_LSB default (reset) value */
    #define ANGLE_RESULT_LSB_DEFAULT                                        ((uint8_t) 0x00)

    /* ANGLE_RESULT_LSB register field masks */
    #define ANGLE_RESULT_LSB_ANGLE_RESULT_MASK                              ((uint8_t) 0xFF)



/* Register 0x1B (MAGNITUDE_RESULT) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |                                                                                     MAGNITUDE_RESULT[7:0]                                                                                     |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* MAGNITUDE_RESULT register address */
    #define MAGNITUDE_RESULT_ADDRESS                                        ((uint8_t) 0x1B)

    /* MAGNITUDE_RESULT default (reset) value */
    #define MAGNITUDE_RESULT_DEFAULT                                        ((uint8_t) 0x00)

    /* MAGNITUDE_RESULT register field masks */
    #define MAGNITUDE_RESULT_MAGNITUDE_RESULT_MASK                          ((uint8_t) 0xFF)



/* Register 0x1C (DEVICE_STATUS) definition
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |         Bit 7         |         Bit 6         |         Bit 5         |         Bit 4         |         Bit 3         |         Bit 2         |         Bit 1         |         Bit 0         |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 * |           0           |           0           |           0           |        INTB_RB        |           0           |         INT_ER        |       OTP_CRC_ER      |       THR_CROSS       |
 * |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
 */

    /* DEVICE_STATUS register address */
    #define DEVICE_STATUS_ADDRESS                                           ((uint8_t) 0x1C)

    /* DEVICE_STATUS default (reset) value */
    #define DEVICE_STATUS_DEFAULT                                           ((uint8_t) 0x10)

    /* DEVICE_STATUS register field masks */
    #define DEVICE_STATUS_INTB_RB_MASK                                      ((uint8_t) 0x10)
    #define DEVICE_STATUS_INT_ER_MASK                                       ((uint8_t) 0x04)
    #define DEVICE_STATUS_OTP_CRC_ER_MASK                                   ((uint8_t) 0x02)
    #define DEVICE_STATUS_THR_CROSS_MASK                                    ((uint8_t) 0x01)

    /* INTB_RB field values */
    #define DEVICE_STATUS_INTB_RB_Low                                       ((uint8_t) 0x00)
    #define DEVICE_STATUS_INTB_RB_High                                      ((uint8_t) 0x10)

    /* INT_ER field values */
    #define DEVICE_STATUS_INT_ER_NoError                                    ((uint8_t) 0x00)
    #define DEVICE_STATUS_INT_ER_INTError                                   ((uint8_t) 0x04)

    /* OTP_CRC_ER field values */
    #define DEVICE_STATUS_OTP_CRC_ER_NoError                                ((uint8_t) 0x00)
    #define DEVICE_STATUS_OTP_CRC_ER_Error                                  ((uint8_t) 0x02)

    /* THR_CROSS field values */
    #define DEVICE_STATUS_THR_CROSS_False                                   ((uint8_t) 0x00)
    #define DEVICE_STATUS_THR_CROSS_True                                    ((uint8_t) 0x01)



#endif /* TMAG3001_H_ */

