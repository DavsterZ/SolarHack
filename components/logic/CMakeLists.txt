idf_component_register(
    SRCS 
    	"src/battery.c" 
    	"src/solar_tracker.c"
    	
    INCLUDE_DIRS 
    	"include"
    	
    PRIV_REQUIRES 
    	sensors 
    	servo_control
)