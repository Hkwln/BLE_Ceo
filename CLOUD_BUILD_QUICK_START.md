# Cloud Build - Quick Start (3 steps)

## 1️⃣ Create Private GitHub Repo

Go to https://github.com/new
- Name: `ble-headphones`
- **Private** ✓
- Do NOT initialize

Copy your repo URL

## 2️⃣ Push Code

```bash
cd /home/openclaw-worker/.openclaw/workspace/ble-headphones

git remote add origin YOUR_REPO_URL
git branch -M main
git push -u origin main
```

**When asked for password:** Use a [Personal Access Token](https://github.com/settings/tokens) (repo scope)

## 3️⃣ Download APK

1. Go to your GitHub repo
2. Click **Actions** tab
3. Wait for build to finish (2-3 min)
4. Click workflow run → **Artifacts** → download APK
5. Install: `adb install app-debug.apk`

---

That's it. Every push auto-builds the APK. Done!
