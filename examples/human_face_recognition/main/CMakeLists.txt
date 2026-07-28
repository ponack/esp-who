set(src_dirs        ./)

set(include_dirs    ./)

set(requires who_spiflash_fatfs
             who_recognition_app
             esp_video_cxx)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})




