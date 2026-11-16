idf_component_register(SRCS "joystick.c" "mixer.c" "ramp.c" "link.c" "state_machine.c"
                            "telem.c" "led_pattern.c"
                       INCLUDE_DIRS "include"
                       REQUIRES protocol)
