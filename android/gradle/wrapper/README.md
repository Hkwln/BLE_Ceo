# Gradle Wrapper JAR

The `gradle-wrapper.jar` should be placed here. It will be automatically downloaded when you run `./gradlew` the first time.

If `./gradlew` fails, download Gradle 8.0 manually:

```bash
# On your build machine (with internet):
curl -O https://services.gradle.org/distributions/gradle-8.0-bin.zip
unzip gradle-8.0-bin.zip

# Or on the machine where you'll build:
cd ble-headphones/android
./gradlew --version  # This will auto-download Gradle
```
