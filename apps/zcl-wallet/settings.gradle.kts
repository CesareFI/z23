// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}
rootProject.name = "ZclassicWallet"
include(":wallet-core", ":android-app", ":scanner-ui-tests")
