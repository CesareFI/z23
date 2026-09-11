// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
plugins {
    kotlin("jvm") version "2.2.21" apply false
    kotlin("android") version "2.2.21" apply false
    id("com.android.application") version "8.13.2" apply false
}
allprojects {
    dependencyLocking { lockAllConfigurations() }
}
