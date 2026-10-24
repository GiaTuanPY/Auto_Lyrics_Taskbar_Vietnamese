(() => {
  const KEY = "__autoLyricsConnector";

  // Nếu có bản content script cũ (ví dụ sau khi extension được nạp lại),
  // dừng nó lại rồi tiếp quản — không cần F5.
  try {
    window[KEY]?.stop?.();
  } catch {
    // Bản cũ đã bị vô hiệu hóa, bỏ qua.
  }

  const POLL_MS = 250;
  const RESEND_MS = 500;
  const SETTLE_MS = 400; // metadata phải ổn định bấy lâu mới coi là bài mới

  let timer = null;
  let stopped = false;

  let lastSignature = "";
  let lastSentAt = 0;
  let candidateKey = "";
  let candidateSince = 0;
  let sentTrackKey = "";

  function clean(value) {
    return (value || "").replace(/\s+/g, " ").trim();
  }

  function getText(selectors) {
    for (const selector of selectors) {
      const element = document.querySelector(selector);
      const value = clean(element?.textContent);
      if (value) return value;
    }
    return "";
  }

  function getVideo() {
    return (
      document.querySelector("video.html5-main-video") ||
      document.querySelector("video.video-stream") ||
      document.querySelector("video")
    );
  }

  function artworkFromMediaSession(metadata) {
    const list = Array.from(metadata.artwork || []);
    if (!list.length) return "";

    const width = (item) => {
      const match = /(\d+)x(\d+)/.exec(item?.sizes || "");
      return match ? Number(match[1]) : 0;
    };

    list.sort((a, b) => width(b) - width(a));
    return list[0]?.src || "";
  }

  function videoIdFromUrl() {
    try {
      return new URL(location.href).searchParams.get("v") || "";
    } catch {
      return "";
    }
  }

  function getMetadata() {
    const video = getVideo();
    if (!video) return null;

    const isMusic = location.hostname === "music.youtube.com";

    let title = "";
    let artist = "";
    let coverUrl = "";

    // 1. Media Session: được YouTube / YouTube Music cập nhật ngay khi đổi bài.
    const session = navigator.mediaSession?.metadata;
    if (session && clean(session.title)) {
      title = clean(session.title);
      artist = clean(session.artist);
      coverUrl = artworkFromMediaSession(session);
    }

    // 2. Dự phòng: đọc từ giao diện.
    if (!title) {
      if (isMusic) {
        title = getText([
          "ytmusic-player-bar .title",
          "ytmusic-player-bar [class*='title']"
        ]);
        artist = getText([
          "ytmusic-player-bar .subtitle a",
          "ytmusic-player-bar .subtitle"
        ]);
      }

      if (!title) {
        title = getText([
          "h1.ytd-watch-metadata yt-formatted-string",
          "h1 yt-formatted-string"
        ]);

        if (!title) {
          title = clean(document.title).replace(/\s*-\s*YouTube\s*$/i, "");
        }
      }
    }

    // YouTube thường chỉ cho biết kênh, không phải nghệ sĩ thật.
    if (!artist) {
      artist = getText([
        "ytd-watch-metadata ytd-channel-name a",
        "#owner #channel-name a",
        "#upload-info #channel-name a"
      ]);
    }

    // Ảnh bìa: nếu thiếu thì dựng từ video id (luôn đúng với bài hiện tại).
    if (!coverUrl) {
      const id = videoIdFromUrl();
      if (id) coverUrl = `https://i.ytimg.com/vi/${id}/hqdefault.jpg`;
    }

    const position = Number(video.currentTime);
    const duration = Number(video.duration);

    return {
      source: "youtube",
      title: title.slice(0, 500),
      artist: artist.slice(0, 300),
      url: location.href.slice(0, 2000),
      cover_url: coverUrl.slice(0, 2000),
      position: Number.isFinite(position) ? Math.max(0, position) : 0,
      duration: Number.isFinite(duration) ? Math.max(0, duration) : 0,
      playing: !video.paused && !video.ended
    };
  }

  function send(data) {
    // Context đã bị vô hiệu hóa (extension vừa nạp lại): tự dừng.
    if (!chrome.runtime?.id) {
      stop();
      return;
    }

    try {
      const result = chrome.runtime.sendMessage({
        type: "AUTO_LYRICS_UPDATE",
        payload: data
      });
      result?.catch?.(() => {
        // Service worker có thể đang khởi động lại; lần poll sau sẽ gửi lại.
      });
    } catch {
      if (!chrome.runtime?.id) stop();
    }
  }

  function poll() {
    if (stopped) return;

    const data = getMetadata();
    if (!data || !data.title) return;

    const now = Date.now();
    const trackKey = [data.title, data.artist, data.url].join("|");

    // Khi bài thay đổi, chờ metadata ổn định để tránh gửi tên bài cũ + URL mới.
    if (trackKey !== candidateKey) {
      candidateKey = trackKey;
      candidateSince = now;
    }

    if (trackKey !== sentTrackKey && now - candidateSince < SETTLE_MS) {
      return;
    }

    const signature = `${trackKey}|${data.playing}`;

    // Gửi mỗi 500 ms; đổi bài / đổi trạng thái thì gửi ngay.
    if (signature === lastSignature && now - lastSentAt < RESEND_MS) return;

    lastSignature = signature;
    lastSentAt = now;
    sentTrackKey = trackKey;

    send(data);
  }

  // Các sự kiện media không nổi bọt, nên nghe ở pha capture.
  const mediaEvents = [
    "loadedmetadata",
    "loadeddata",
    "emptied",
    "play",
    "playing",
    "pause",
    "seeked",
    "ended"
  ];

  const pageEvents = [
    "yt-navigate-finish",
    "yt-page-data-updated",
    "visibilitychange"
  ];

  function stop() {
    if (stopped) return;
    stopped = true;

    if (timer !== null) clearInterval(timer);

    for (const name of mediaEvents) {
      document.removeEventListener(name, poll, true);
    }
    for (const name of pageEvents) {
      document.removeEventListener(name, poll, true);
      window.removeEventListener(name, poll, true);
    }
  }

  timer = setInterval(poll, POLL_MS);

  for (const name of mediaEvents) {
    document.addEventListener(name, poll, true);
  }
  for (const name of pageEvents) {
    document.addEventListener(name, poll, true);
    window.addEventListener(name, poll, true);
  }

  window[KEY] = { stop };
  poll();
})();