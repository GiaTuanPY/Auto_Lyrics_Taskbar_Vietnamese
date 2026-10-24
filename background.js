const ENDPOINT = "http://127.0.0.1:8765/nowplaying";
const ALLOWED_HOSTS = ["www.youtube.com", "music.youtube.com"];

chrome.runtime.onMessage.addListener((message) => {
  if (message?.type !== "AUTO_LYRICS_UPDATE") return;

  const data = message.payload;

  if (
    !data ||
    data.source !== "youtube" ||
    typeof data.title !== "string" ||
    typeof data.url !== "string"
  ) {
    return;
  }

  // Chỉ nhận hai domain YouTube được hỗ trợ.
  let parsedUrl;
  try {
    parsedUrl = new URL(data.url);
  } catch {
    return;
  }

  if (!ALLOWED_HOSTS.includes(parsedUrl.hostname)) return;

  fetch(ENDPOINT, {
    method: "POST",
    headers: {
      "Content-Type": "application/json"
    },
    body: JSON.stringify(data)
  }).catch(() => {
    // Overlay có thể chưa chạy. Content script sẽ gửi lại sau 500 ms.
  });
});

// Khi extension được cài / cập nhật / nạp lại, gắn lại content script vào các
// tab YouTube đang mở để không phải F5.
chrome.runtime.onInstalled.addListener(async () => {
  try {
    const tabs = await chrome.tabs.query({
      url: [
        "https://www.youtube.com/*",
        "https://music.youtube.com/*"
      ]
    });

    for (const tab of tabs) {
      if (tab.id === undefined) continue;

      chrome.scripting
        .executeScript({
          target: { tabId: tab.id },
          files: ["content.js"]
        })
        .catch(() => {
          // Tab chưa sẵn sàng hoặc không được phép chèn script.
        });
    }
  } catch {
    // Bỏ qua: content script sẽ tự nạp ở lần tải trang kế tiếp.
  }
});