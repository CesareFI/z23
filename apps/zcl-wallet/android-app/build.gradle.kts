// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
plugins {
    id("com.android.application")
    kotlin("android")
}
android {
    buildFeatures { aidl = true }
    namespace = "org.zclassic.wallet"
    compileSdk = 36
    ndkVersion = "27.2.12479018"
    defaultConfig {
        applicationId = "org.zclassic.wallet.dev"
        minSdk = 30
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0-dev"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        ndk { abiFilters += listOf("arm64-v8a", "x86_64") }
        externalNativeBuild { cmake {
            arguments += listOf("-DZCL_JNI=ON", "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON")
        } }
    }
    externalNativeBuild {
        cmake {
            path = file("../native/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    sourceSets.getByName("androidTest").assets.srcDir("../wallet-core/src/test/resources")
    sourceSets.getByName("test").resources.srcDir("../wallet-core/src/test/resources")
    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    lint {
        abortOnError = true
        warningsAsErrors = true
    }
}
kotlin { compilerOptions { allWarningsAsErrors.set(true) } }
tasks.withType<org.gradle.api.tasks.testing.Test>().configureEach {
    dependsOn(":wallet-core:buildHostNative")
    systemProperty("java.library.path", rootProject.file("native/build/jni").absolutePath)
    jvmArgs("-Xcheck:jni")
    inputs.files(rootProject.fileTree("native/build/jni") {
        include("libzclwallet_jni.so", "libzclwallet_jni.dylib")
    }).withPathSensitivity(PathSensitivity.RELATIVE)
}
val checkFixtureIsolation by tasks.registering(Exec::class) {
    group = "verification"
    description = "Verify built APK fixture separation and negative manifest regressions."
    dependsOn("assembleDebug", "assembleDebugAndroidTest", "assembleRelease")
    workingDir(rootProject.projectDir)
    commandLine("bash", rootProject.file("tools/test-android-fixtures.sh"),
        android.sdkDirectory.resolve("build-tools/${android.buildToolsVersion}/aapt2"),
        layout.buildDirectory.dir("reports/fixture-isolation").get().asFile)
}
val checkNativeAlignment by tasks.registering(Exec::class) {
    group = "verification"
    description = "Verify both APKs and native ABIs support 16 KiB page alignment."
    dependsOn("assembleDebug", "assembleRelease")
    workingDir(rootProject.projectDir)
    val osName = System.getProperty("os.name")
    val ndkHost = when {
        osName.startsWith("Mac") -> "darwin-x86_64"
        osName.startsWith("Windows") -> "windows-x86_64"
        else -> "linux-x86_64"
    }
    val executableSuffix = if (osName.startsWith("Windows")) ".exe" else ""
    commandLine("bash", rootProject.file("tools/check-android-native.sh"),
        android.sdkDirectory.resolve("build-tools/${android.buildToolsVersion}/zipalign$executableSuffix"),
        android.sdkDirectory.resolve("ndk/${android.ndkVersion}/toolchains/llvm/prebuilt/$ndkHost/bin/llvm-readelf$executableSuffix"),
        layout.buildDirectory.file("outputs/apk/debug/android-app-debug.apk").get().asFile,
        layout.buildDirectory.file("outputs/apk/release/android-app-release-unsigned.apk").get().asFile,
        layout.buildDirectory.dir("reports/native-alignment").get().asFile)
}
tasks.named("check") { dependsOn(checkFixtureIsolation, checkNativeAlignment) }
dependencies {
    implementation(project(":wallet-core"))
    testImplementation(kotlin("test-junit"))
    androidTestImplementation("androidx.test:runner:1.7.0")
    androidTestImplementation("androidx.test.ext:junit:1.3.0")
    androidTestImplementation("com.google.zxing:core:3.5.4")
}
