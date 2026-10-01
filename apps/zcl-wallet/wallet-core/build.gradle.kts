// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
plugins { kotlin("jvm") }
kotlin {
    jvmToolchain(17)
    compilerOptions { allWarningsAsErrors.set(true) }
}
dependencies {
    testImplementation(kotlin("test-junit5"))
    testImplementation("com.google.zxing:core:3.5.4")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher:1.12.2")
}
tasks.test {
    useJUnitPlatform()
    maxHeapSize = "512m"
    dependsOn("buildHostNative")
    systemProperty("java.library.path", rootProject.file("native/build/jni").absolutePath)
    jvmArgs("-Xcheck:jni")
    inputs.files(rootProject.fileTree("native") {
        include("src/**", "include/**", "vendor/**", "tests/jni_secret_output_fixture.c", "CMakeLists.txt")
    }).withPathSensitivity(PathSensitivity.RELATIVE)
    inputs.files(rootProject.fileTree("../../vendor/android-mbedtls")).withPathSensitivity(PathSensitivity.RELATIVE)
    inputs.files(rootProject.fileTree("../../vendor/android-bip39")).withPathSensitivity(PathSensitivity.RELATIVE)
    inputs.files(rootProject.fileTree("../../vendor/android-secp256k1")).withPathSensitivity(PathSensitivity.RELATIVE)
    inputs.files(rootProject.fileTree("../../vendor/android-qrcodegen")).withPathSensitivity(PathSensitivity.RELATIVE)
    inputs.files(rootProject.fileTree("../../vendor/android-quirc")).withPathSensitivity(PathSensitivity.RELATIVE)
    inputs.files(rootProject.fileTree("../../contexts/commons/packages/zjsonp") {
        include("src/**", "include/**")
    }).withPathSensitivity(PathSensitivity.RELATIVE)
    inputs.files(rootProject.fileTree("../../contexts/commons/packages/zutf8") {
        include("src/**", "include/**")
    }).withPathSensitivity(PathSensitivity.RELATIVE)
    inputs.file(rootProject.file("native/json-provider.sha256")).withPathSensitivity(PathSensitivity.RELATIVE)
}

val configureHostNative by tasks.registering(Exec::class) {
    commandLine("cmake", "-S", rootProject.file("native"), "-B", rootProject.file("native/build/jni"),
        "-DZCL_JNI=ON", "-DCMAKE_BUILD_TYPE=Debug")
}
tasks.register<Exec>("buildHostNative") {
    dependsOn(configureHostNative)
    // The host fixture depends on the JNI library and all of its C providers.
    // Standalone native suites keep their separate canonical safety build.
    commandLine("cmake", "--build", rootProject.file("native/build/jni"),
        "--target", "zclwallet_secret_fixture", "-j4")
}
