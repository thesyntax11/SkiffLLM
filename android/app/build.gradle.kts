plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "com.llm.app"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.llm.app"
        minSdk = 26
        targetSdk = 34
        versionCode = 10600
        versionName = "1.6.0"

        ndkVersion = "26.3.11579264"

        ndk {
            // 64-bit only: the pinned llama.cpp sgemm kernel uses FP16 NEON
            // intrinsics that ARMv7 (armeabi-v7a) does not provide, and modern
            // quantized LLM inference on 32-bit ARM is not practical. Hosts in
            // 2026 are overwhelmingly arm64; x86_64 is kept for emulators.
            abiFilters += listOf("arm64-v8a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++17"
                arguments += "-DANDROID_STL=c++_shared"
                arguments += "-DCMAKE_BUILD_TYPE=Release"
                val llamaDir = project.findProperty("llm.llamaSourceDir") as String?
                if (llamaDir != null && llamaDir.isNotBlank()) {
                    arguments += "-DLLM_LLAMA_SOURCE_DIR=$llamaDir"
                }
                // GPU/NPU acceleration on Android: `./gradlew ... -Pllm.backend=vulkan|opencl|cpu`
                val backend = project.findProperty("llm.backend") as String?
                if (!backend.isNullOrBlank()) {
                    arguments += "-DLLM_LLAMA_BACKEND=$backend"
                }
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    buildFeatures {
        compose = true
        buildConfig = true
    }

    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
        jniLibs {
            useLegacyPackaging = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }
}

dependencies {
    val composeBom = platform("androidx.compose:compose-bom:2024.09.02")
    implementation(composeBom)
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.activity:activity-compose:1.9.2")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.6")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.compose.foundation:foundation")
    implementation("androidx.compose.material3:material3")
    debugImplementation("androidx.compose.ui:ui-tooling")
}
