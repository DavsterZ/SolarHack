idf_component_register(
    SRCS 
    	"main.c"
    	
    INCLUDE_DIRS 
    	"."
    
    REQUIRES 
    	sensors 
    	servo_control 
    	connectivity 
    	logic
)
