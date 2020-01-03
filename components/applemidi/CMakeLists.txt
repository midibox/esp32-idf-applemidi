set(COMPONENT_SRCS "applemidi.c if/lwip/applemidi_if.c")
set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_REQUIRES lwip console)
register_component()