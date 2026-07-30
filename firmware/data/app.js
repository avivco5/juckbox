'use strict';

// ── State ────────────────────────────────────────────────────────────────────
let isPlaying    = false;
let currentPath  = '';
let pollTimer    = null;
let volumeTimer  = null;

// ── Init ─────────────────────────────────────────────────────────────────────
document.addEventListener('DOMContentLoaded', () => {
  loadStatus();
  loadSongs();
  loadPlaylists();
  loadLogs();
  startPolling();
  initUploadZone();
});

function startPolling() {
  pollTimer = setInterval(loadStatus, 2000);
}

// ── Status polling ────────────────────────────────────────────────────────────
async function loadStatus() {
  try {
    const data = await getJson('/api/status');
    applyStatus(data);
  } catch {
    setChip(false);
  }
}

function applyStatus(d) {
  setChip(d.connected);

  // Now Playing
  setText('current-song', d.currentSong || '—');
  setText('play-status',  d.isPlaying ? '▶ מתנגן' : '■ עצור');

  const btnPlay = id('btn-play');
  btnPlay.innerHTML    = d.isPlaying ? '⏸' : '▶';
  btnPlay.classList.toggle('playing', d.isPlaying);

  // Volume (only update if user is not dragging)
  if (document.activeElement !== id('volume-slider')) {
    id('volume-slider').value = d.volume;
    setText('vol-display', d.volume);
  }

  // System
  setText('sd-status',     d.sdMounted    ? '✓ מחובר' : '✗ חסר');
  setText('sd-song-count', d.songCount ?? '—');
  setText('free-heap',     fmtBytes(d.freeHeap));
  setText('uptime',        fmtUptime(d.uptime));

  isPlaying   = d.isPlaying;
  currentPath = d.currentSong ? ('/songs/' + d.currentSong) : '';
}

function setChip(online) {
  const el = id('status-chip');
  el.textContent = online ? 'מחובר' : 'מנותק';
  el.className   = 'chip ' + (online ? 'chip-online' : 'chip-offline');
}

// ── Song list ─────────────────────────────────────────────────────────────────
async function loadSongs() {
  const list = id('song-list');
  list.innerHTML = '<p class="hint">טוען...</p>';

  try {
    const songs = await getJson('/api/songs');

    const badge = id('song-count-badge');
    if (songs.length === 0) {
      list.innerHTML = '<p class="hint">אין שירים. העלה קבצי MP3 / WAV.</p>';
      badge.style.display = 'none';
      return;
    }

    badge.textContent   = songs.length + ' שירים';
    badge.style.display = 'inline-block';

    list.innerHTML = songs.map((s, i) => `
      <div class="song-item${s.path === currentPath ? ' active' : ''}"
           onclick="playSong('${esc(s.path)}','${esc(s.name)}')">
        <span class="song-num">${i + 1}</span>
        <div class="song-info">
          <span class="song-name">${esc(s.name)}</span>
          <span class="song-size">${fmtBytes(s.size)}</span>
        </div>
        <span class="song-play">▶</span>
      </div>`).join('');

  } catch (e) {
    list.innerHTML = '<p class="hint txt-error">שגיאה בטעינת הרשימה</p>';
  }
}

async function playSong(path, name) {
  try {
    await postJson('/api/control', { action: 'play', song: path });
    currentPath = path;
    await loadStatus();
    await loadSongs();      // refresh active highlight
  } catch (e) {
    console.error('playSong error', e);
  }
}

// ── Player controls ───────────────────────────────────────────────────────────
async function togglePlay() {
  await control(isPlaying ? 'pause' : 'play');
}

async function control(action) {
  try {
    await postJson('/api/control', { action });
    await loadStatus();
  } catch (e) {
    console.error('control error', e);
  }
}

// ── Volume ────────────────────────────────────────────────────────────────────
function onVolumeInput(val) {
  setText('vol-display', val);
  clearTimeout(volumeTimer);
  volumeTimer = setTimeout(() => {
    postJson('/api/control', { action: 'setVolume', volume: parseInt(val) })
      .catch(console.error);
  }, 300);
}

// ── Playlists ─────────────────────────────────────────────────────────────────
async function loadPlaylists() {
  const list = id('playlist-list');
  try {
    const playlists = await getJson('/api/playlists');
    if (!playlists.length) {
      list.innerHTML = '<p class="hint">אין פלייליסטים</p>';
      return;
    }
    list.innerHTML = playlists.map(p => `
      <div class="playlist-item"
           onclick="openPlaylist('${esc(p.filename)}')">
        <span class="playlist-name">${esc(p.name)}</span>
        <span class="playlist-count">${p.songCount} שירים</span>
      </div>`).join('');
  } catch {
    list.innerHTML = '<p class="hint txt-error">שגיאה</p>';
  }
}

async function openPlaylist(filename) {
  try {
    const pl = await getJson('/api/playlist?name=' + encodeURIComponent(filename));
    if (pl.songs && pl.songs.length > 0) {
      // Play the first song in the playlist
      await playSong(pl.songs[0], pl.name);
    }
  } catch (e) {
    console.error('openPlaylist error', e);
  }
}

// ── File Upload ───────────────────────────────────────────────────────────────
function initUploadZone() {
  const zone  = id('upload-zone');
  const input = id('file-input');

  // Drag-and-drop
  zone.addEventListener('dragover', e => {
    e.preventDefault();
    zone.classList.add('active');
  });
  zone.addEventListener('dragleave', () => zone.classList.remove('active'));
  zone.addEventListener('drop', e => {
    e.preventDefault();
    zone.classList.remove('active');
    onFilesChosen(e.dataTransfer.files);
  });
}

function onFilesChosen(files) {
  if (!files || files.length === 0) return;
  uploadQueue([...files], 0);
}

function uploadQueue(files, idx) {
  if (idx >= files.length) {
    showUploadStatus('כל הקבצים הועלו ✓', 'success');
    id('upload-progress-wrap').style.display = 'none';
    id('file-input').value = '';
    loadSongs();
    return;
  }
  uploadSingleFile(files[idx], () => uploadQueue(files, idx + 1));
}

function uploadSingleFile(file, onDone) {
  const ext = file.name.split('.').pop().toLowerCase();
  if (ext !== 'mp3' && ext !== 'wav') {
    showUploadStatus(`${esc(file.name)}: סוג קובץ לא נתמך`, 'error');
    onDone();
    return;
  }

  const wrap   = id('upload-progress-wrap');
  const fill   = id('progress-fill');
  const label  = id('progress-label');

  wrap.style.display = 'block';
  fill.style.width   = '0%';
  label.textContent  = `מעלה: ${file.name} – 0%`;
  showUploadStatus('', '');

  const form = new FormData();
  form.append('file', file, file.name);

  const xhr = new XMLHttpRequest();

  xhr.upload.onprogress = e => {
    if (e.lengthComputable) {
      const pct = Math.round((e.loaded / e.total) * 100);
      fill.style.width  = pct + '%';
      label.textContent = `מעלה: ${file.name} – ${pct}%`;
    }
  };

  xhr.onload = () => {
    if (xhr.status === 200) {
      showUploadStatus(`✓ ${esc(file.name)} הועלה`, 'success');
    } else {
      let msg = 'שגיאה';
      try { msg = JSON.parse(xhr.responseText).error || msg; } catch {}
      showUploadStatus(`✗ ${esc(file.name)}: ${msg}`, 'error');
    }
    fill.style.width  = '100%';
    label.textContent = '100%';
    onDone();
  };

  xhr.onerror = () => {
    showUploadStatus('שגיאת חיבור בהעלאה', 'error');
    onDone();
  };

  xhr.open('POST', '/api/upload');
  xhr.send(form);
}

function showUploadStatus(msg, type) {
  const el = id('upload-status');
  el.textContent = msg;
  el.className   = 'upload-status' + (type ? ' txt-' + type : '');
}

// ── Logs ──────────────────────────────────────────────────────────────────────
async function loadLogs() {
  try {
    const data   = await getJson('/api/logs');
    const logsEl = id('logs');
    if (data.logs && data.logs.length) {
      logsEl.innerHTML = data.logs
        .map(l => `<div class="log-line">${esc(l)}</div>`)
        .join('');
      logsEl.scrollTop = logsEl.scrollHeight;
    } else {
      logsEl.innerHTML = '<span class="hint">אין לוגים</span>';
    }
  } catch {}
}

// ── HTTP helpers ──────────────────────────────────────────────────────────────
async function getJson(url) {
  const r = await fetch(url);
  if (!r.ok) throw new Error(`HTTP ${r.status}`);
  return r.json();
}

async function postJson(url, body) {
  const r = await fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  });
  if (!r.ok) throw new Error(`HTTP ${r.status}`);
  return r.json();
}

// ── Utility ───────────────────────────────────────────────────────────────────
function id(x)       { return document.getElementById(x); }
function setText(x,t){ const e=id(x); if(e) e.textContent=t; }

function esc(s) {
  return String(s)
    .replace(/&/g,'&amp;').replace(/</g,'&lt;')
    .replace(/>/g,'&gt;').replace(/"/g,'&quot;')
    .replace(/'/g,'&#39;');
}

function fmtBytes(b) {
  if (b == null) return '—';
  if (b < 1024)       return b + ' B';
  if (b < 1048576)    return (b / 1024).toFixed(1) + ' KB';
  return (b / 1048576).toFixed(1) + ' MB';
}

function fmtUptime(sec) {
  if (sec == null) return '—';
  const h = Math.floor(sec / 3600);
  const m = Math.floor((sec % 3600) / 60);
  const s = sec % 60;
  if (h > 0) return `${h}h ${m}m`;
  if (m > 0) return `${m}m ${s}s`;
  return `${s}s`;
}
