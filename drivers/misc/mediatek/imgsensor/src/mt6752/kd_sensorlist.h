//s_add new sensor driver here
//export funtions

UINT32 IMX219_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc);
UINT32 OV5670_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc);
UINT32 OV8858_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc);
UINT32 OV8858R1A_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc);
UINT32 OV13850_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc);
UINT32 GC2235_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc);
UINT32 GC2235_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc);
UINT32 GC2355_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc);

//! Add Sensor Init function here
//! Note:
//! 1. Add by the resolution from ""large to small"", due to large sensor
//!    will be possible to be main sensor.
//!    This can avoid I2C error during searching sensor.
//! 2. This file should be the same as mediatek\custom\common\hal\imgsensor\src\sensorlist.cpp
ACDK_KD_SENSOR_INIT_FUNCTION_STRUCT kdSensorList[MAX_NUM_OF_SUPPORT_SENSOR+1] =
{
#if defined(IMX219_MIPI_RAW)
    {IMX219_SENSOR_ID, SENSOR_DRVNAME_IMX219_MIPI_RAW, IMX219_MIPI_RAW_SensorInit}, 
#endif
#if defined(OV5670_MIPI_RAW)
    {OV5670MIPI_SENSOR_ID, SENSOR_DRVNAME_OV5670_MIPI_RAW, OV5670_MIPI_RAW_SensorInit}, 
#endif
#if defined(OV8858_MIPI_RAW)
    {OV8858_SENSOR_ID, SENSOR_DRVNAME_OV8858_MIPI_RAW,OV8858_MIPI_RAW_SensorInit}, 
#endif
#if defined(OV8858_MIPI_RAW_R1A)
    {OV8858R1A_SENSOR_ID, SENSOR_DRVNAME_OV8858R1A_MIPI_RAW,OV8858R1A_MIPI_RAW_SensorInit}, 
#endif
#if defined(OV13850_MIPI_RAW)
    {OV13850_SENSOR_ID, SENSOR_DRVNAME_OV13850_MIPI_RAW,OV13850_MIPI_RAW_SensorInit},
#endif
#if defined(GC2235_RAW)
    {GC2235_SENSOR_ID, SENSOR_DRVNAME_GC2235_RAW,GC2235_RAW_SensorInit},
#endif
#if defined(GC2235_MIPI_RAW)
    {GC2235MIPI_SENSOR_ID, SENSOR_DRVNAME_GC2235_MIPI_RAW,GC2235_MIPI_RAW_SensorInit},
#endif
#if defined(GC2355_MIPI_RAW)
    {GC2355_SENSOR_ID, SENSOR_DRVNAME_GC2355_MIPI_RAW,GC2355_MIPI_RAW_SensorInit}, 
#endif


/*  ADD sensor driver before this line */
    {0,{0},NULL}, //end of list
};
//e_add new sensor driver here

