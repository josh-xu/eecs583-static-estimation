add_library(StaticEstimator MODULE
    # List your source files here.
    lib/StaticEstimator.cpp
    lib/FeatureExtractor.cpp
    lib/OpStatCounter.cpp
    lib/BLInstrumentation.cpp
)

add_library(FeatureExtractorHarness MODULE
    # List your source files here.
    lib/FeatureExtractorHarness.cpp
    lib/FeatureExtractor.cpp
    lib/OpStatCounter.cpp
    lib/BLInstrumentation.cpp
)

include_directories(include)

# Use C++11 to compile our pass (i.e., supply -std=c++11).
target_compile_features(StaticEstimator PRIVATE cxx_range_for cxx_auto_type)
target_compile_features(FeatureExtractorHarness PRIVATE cxx_range_for cxx_auto_type)

# LLVM is (typically) built with no C++ RTTI. We need to match that.
set_target_properties(StaticEstimator PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
)
set_target_properties(FeatureExtractorHarness PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
)

# Get proper shared-library behavior (where symbols are not necessarily
# resolved when the shared library is linked) on OS X.
if(APPLE)
    set_target_properties(StaticEstimator PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
    )
set_target_properties(FeatureExtractorHarness PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
    )
endif(APPLE)
