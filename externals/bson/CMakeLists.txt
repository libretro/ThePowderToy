add_library(bson STATIC
        BSON.cpp
        BSON.h
        tpt-inline.h)

create_target_directory_groups(bson)
