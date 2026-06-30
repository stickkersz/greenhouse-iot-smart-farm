// SmartFarm Service Worker v1.4
const CACHE = 'smartfarm-v1.4';
const PRECACHE = ['/index.html', '/manifest.json', '/icon.svg'];

self.addEventListener('install', e => {
  e.waitUntil(caches.open(CACHE).then(c => c.addAll(PRECACHE)));
  self.skipWaiting();
});

self.addEventListener('activate', e => {
  e.waitUntil(
    caches.keys().then(keys =>
      Promise.all(keys.filter(k => k !== CACHE).map(k => caches.delete(k)))
    )
  );
  self.clients.claim();
});

self.addEventListener('fetch', e => {
  const req = e.request;
  if (req.method !== 'GET') return;

  // HTML/navigation → network-first: ได้ dashboard เวอร์ชันใหม่เสมอเมื่อออนไลน์,
  // fallback ไป cache เฉพาะตอนออฟไลน์
  const isHTML = req.mode === 'navigate' ||
    (req.headers.get('accept') || '').includes('text/html');
  if (isHTML) {
    e.respondWith(
      fetch(req)
        .then(res => {
          const copy = res.clone();
          caches.open(CACHE).then(c => c.put('/index.html', copy));
          return res;
        })
        .catch(() => caches.match('/index.html'))
    );
    return;
  }

  // ไฟล์อื่น (icon/manifest) → cache-first
  e.respondWith(caches.match(req).then(cached => cached || fetch(req)));
});

// Push notification (for future FCM integration)
self.addEventListener('push', e => {
  const data = e.data?.json() ?? { title: 'SmartFarm', body: '⚠️ แจ้งเตือนจากระบบ' };
  e.waitUntil(
    self.registration.showNotification(data.title, {
      body:      data.body,
      icon:      '/icon.svg',
      badge:     '/icon.svg',
      vibrate:   [200, 100, 200],
      tag:       'smartfarm-alert',
      renotify:  true,
    })
  );
});
