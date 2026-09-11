// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
plugins { kotlin("jvm") }
kotlin {
    jvmToolchain(17)
    compilerOptions { allWarningsAsErrors.set(true) }
}
dependencies {
    testImplementation(kotlin("test-junit5"))
    testRuntimeOnly("org.junit.platform:junit-platform-launcher:1.12.2")
}
tasks.test {
    useJUnitPlatform()
    maxHeapSize = "512m"
    dependsOn("buildHostNative")
    systemProperty("java.library.path", rootProject.file("native/build/jni").absolutePath)
    jvmArgs("-Xcheck:jni")
    inputs.files(rootProject.fileTree("native") {
        include("src/**", "include/**", "vendor/**", "CMakeLists.txt")
    }).withPathSensitivity(PathSensitivity.RELATIVE)
    inputs.files(rootProject.fileTree("../../vendor/android-mbedtls")).withPathSensitivity(PathSensitivity.RELATIVE)
}

val configureHostNative by tasks.registering(Exec::class) {
    commandLine("cmake", "-S", rootProject.file("native"), "-B", rootProject.file("native/build/jni"),
        "-DZCL_JNI=ON", "-DCMAKE_BUILD_TYPE=Debug")
}
tasks.register<Exec>("buildHostNative") {
    dependsOn(configureHostNative)
    commandLine("cmake", "--build", rootProject.file("native/build/jni"), "-j4")
}
