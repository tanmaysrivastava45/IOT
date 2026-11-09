# Smart IoT Irrigation Cloud Service

Node.js + Express backend and live dashboard for the ESP32 Smart IoT Irrigation System.

## Free Deployment to Render (Step-by-Step)

### 1. Put this folder on GitHub
1. Create a new repository on your GitHub account (e.g. `smart-irrigation-cloud`).
2. Run these commands in this directory:
   ```bash
   git init
   git add .
   git commit -m "Initial commit for Smart Irrigation Cloud"
   git branch -M main
   git remote add origin https://github.com/YOUR_USERNAME/smart-irrigation-cloud.git
   git push -u origin main
   ```

### 2. Deploy on Render
1. Go to [https://render.com](https://render.com) and sign in with GitHub.
2. Click **New +** -> **Web Service**.
3. Select your `smart-irrigation-cloud` repository.
4. Fill in these simple fields:
   * **Name:** `smart-irrigation-cloud` (or whatever you prefer)
   * **Region:** Any (e.g., Singapore or Oregon)
   * **Branch:** `main`
   * **Runtime:** `Node`
   * **Build Command:** `npm install`
   * **Start Command:** `node server.js`
   * **Instance Type:** `Free`
5. Click **Deploy Web Service**!

Render will build and give you a free public URL, for example:
👉 `https://smart-irrigation-cloud-xxxx.onrender.com`

---

## Testing Locally
Run locally on your laptop:
```bash
npm start
```
And open `http://localhost:3000` in your browser.

