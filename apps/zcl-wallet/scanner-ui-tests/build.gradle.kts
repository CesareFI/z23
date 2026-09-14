// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
plugins {
    id("com.android.application")
    kotlin("android")
}

// A separate instrumentation APK supplies its own Kotlin/test runtime. The
// production APK is built normally, with no test-driven R8 keeps or mapping.
android {
    namespace = "org.zclassic.wallet.scannerfixture"
    compileSdk = 36
    defaultConfig {
        applicationId = "org.zclassic.wallet.scannerfixture"
        minSdk = 30
        targetSdk = 36
        versionCode = 1
        versionName = "1-test-only"
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    lint { abortOnError = true; warningsAsErrors = true }
}
kotlin { compilerOptions { allWarningsAsErrors.set(true) } }
dependencies {
    implementation("androidx.test:runner:1.7.0")
    implementation("junit:junit:4.13.2")
}
