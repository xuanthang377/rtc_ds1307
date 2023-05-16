set(COMPONENT_SRCS main.c ds1307.c i2cdev.c)
set(COMPONENT_ADD_INCLUDEDIRS "")

idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS ".")
