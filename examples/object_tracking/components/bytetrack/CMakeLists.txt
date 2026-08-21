idf_component_register(SRCS "src/BYTETracker.cpp"
                            "src/STrack.cpp"
                            "src/kalmanFilter.cpp"
                            "src/lapjv.cpp"
                            "src/utils.cpp"
                       INCLUDE_DIRS "include")

# Eigen is header-only: include it directly from the managed espressif/eigen
# component sources. (Its own CMake installs headers under
# install/include/eigen3 while exporting install/include, so <Eigen/Core>
# does not resolve through the regular interface include path.)
target_include_directories(${COMPONENT_LIB} PUBLIC
                           "${PROJECT_DIR}/managed_components/espressif__eigen/eigen")
