# GitHub Cloud Build Setup

Everything is ready for automated builds. Here's how to set it up:

## Step 1: Create a Private GitHub Repo

1. Go to https://github.com/new
2. Create a new **private** repository (e.g., `ble-headphones`)
3. Do NOT initialize with README (we have our own)
4. Note your repo URL (e.g., `https://github.com/your-username/ble-headphones.git`)

## Step 2: Push to GitHub

On your machine where you have the code:

```bash
cd /path/to/ble-headphones

# Add GitHub as remote
git remote add origin https://github.com/your-username/ble-headphones.git

# Push to GitHub
git branch -M main
git push -u origin main
```

If asked for password: **Use a Personal Access Token** (not your GitHub password)
- Go to https://github.com/settings/tokens
- Create token with `repo` scope
- Use as password when pushing

## Step 3: Verify Workflow

1. Go to your GitHub repo
2. Click **Actions** tab
3. You should see "Build Android APK" workflow running
4. Wait for it to complete (2-3 minutes)
5. Once done, click the workflow run and download the APK from **Artifacts**

## Step 4: Download & Install APK

After each build:

1. Go to **Actions** tab
2. Click latest workflow run
3. Scroll down to **Artifacts**
4. Download `ble-headphones-debug.zip`
5. Extract and install:
   ```bash
   adb install ble-headphones-debug/app-debug.apk
   ```

## Auto-Build Triggers

The workflow **automatically builds** when:
- You push to `main` or `master` branch
- You create a pull request
- You manually trigger via **Actions → Build Android APK → Run workflow**

## What's Included

The workflow:
- ✅ Checks out your code
- ✅ Sets up Java 11
- ✅ Downloads Gradle
- ✅ Builds APK with `./gradlew assembleDebug`
- ✅ Uploads APK as artifact (kept for 30 days)
- ✅ Auto-creates GitHub releases for version tags

## Example Workflow

```
1. Make code changes locally
2. Commit & push to GitHub
3. GitHub Actions automatically builds APK
4. Download APK from workflow artifacts
5. Install on phone with `adb install`
```

## Optional: Release Tags

To create a versioned release:

```bash
git tag v1.0.0
git push origin v1.0.0
```

Then GitHub automatically creates a release with the APK attached.

## Troubleshooting

**Build fails?**
- Check **Actions** tab for error logs
- Common issues:
  - Missing Java JDK (workflow handles this)
  - Gradle cache issues (usually resolves on retry)
  - SDK version mismatch (update build.gradle)

**APK not in artifacts?**
- Build may have failed — check logs
- Wait a few minutes, refresh page

**Can't download APK?**
- Make sure repo is public OR you're logged into GitHub
- Artifacts are private to repo collaborators

---

**That's it! You now have automated cloud builds.** Every push automatically builds your APK.
