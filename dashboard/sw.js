// SmartFarm Service Worker v1.2
const CACHE = 'smartfarm-v1.2';
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
  if (e.request.method !== 'GET') return;
  e.respondWith(
    caches.match(e.request).then(cached => cached || fetch(e.request))
  );
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
