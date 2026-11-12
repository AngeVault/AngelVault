/* AngeVault Engine — comportements du site (thème, menus, sommaire, copie de code) */
(function () {
  'use strict';
  var root = document.documentElement;

  /* ---- Thème clair / sombre ---- */
  function safeGet(k) { try { return localStorage.getItem(k); } catch (e) { return null; } }
  function safeSet(k, v) { try { localStorage.setItem(k, v); } catch (e) { /* ignoré */ } }
  var themeBtn = document.getElementById('theme-btn');
  function paintThemeIcon() {
    if (!themeBtn) return;
    var light = root.getAttribute('data-theme') === 'light';
    var moon = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8z"/></svg>';
    var sun = '<svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/></svg>';
    themeBtn.innerHTML = light ? moon : sun;
    themeBtn.setAttribute('aria-label', light ? 'Passer au thème sombre' : 'Passer au thème clair');
  }
  paintThemeIcon();
  if (themeBtn) themeBtn.addEventListener('click', function () {
    var next = root.getAttribute('data-theme') === 'light' ? 'dark' : 'light';
    root.setAttribute('data-theme', next);
    safeSet('av-theme', next);
    paintThemeIcon();
  });

  /* ---- Menu mobile + sous-menus ---- */
  var nav = document.getElementById('nav');
  var burger = document.getElementById('burger');
  if (burger && nav) burger.addEventListener('click', function () {
    var open = nav.classList.toggle('open');
    burger.setAttribute('aria-expanded', open ? 'true' : 'false');
  });
  var toggles = document.querySelectorAll('.nav button.top');
  function closeAll(except) {
    document.querySelectorAll('.nav .item.open').forEach(function (it) {
      if (it !== except) { it.classList.remove('open'); var b = it.querySelector('button.top'); if (b) b.setAttribute('aria-expanded', 'false'); }
    });
  }
  toggles.forEach(function (btn) {
    btn.addEventListener('click', function (e) {
      e.stopPropagation();
      var item = btn.parentElement;
      var open = item.classList.toggle('open');
      btn.setAttribute('aria-expanded', open ? 'true' : 'false');
      closeAll(item);
    });
  });
  document.addEventListener('click', function () { closeAll(null); });
  document.addEventListener('keydown', function (e) { if (e.key === 'Escape') closeAll(null); });

  /* ---- Copier le code ---- */
  document.querySelectorAll('.copy').forEach(function (btn) {
    btn.addEventListener('click', function () {
      var pre = btn.closest('.code').querySelector('pre');
      var text = pre.innerText;
      function done() { var old = btn.textContent; btn.textContent = 'Copié ✓'; setTimeout(function () { btn.textContent = old; }, 1400); }
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(done, function () { fallback(text, done); });
      } else { fallback(text, done); }
    });
  });
  function fallback(text, cb) {
    var ta = document.createElement('textarea');
    ta.value = text; ta.style.position = 'fixed'; ta.style.opacity = '0';
    document.body.appendChild(ta); ta.select();
    try { document.execCommand('copy'); cb(); } catch (e) { /* ignoré */ }
    document.body.removeChild(ta);
  }

  /* ---- Sommaire : surbrillance de la section visible ---- */
  var links = document.querySelectorAll('.toc a');
  if (links.length && 'IntersectionObserver' in window) {
    var map = {};
    links.forEach(function (a) { map[a.getAttribute('href').slice(1)] = a; });
    var heads = Array.prototype.slice.call(document.querySelectorAll('.prose h2[id], .prose h3[id]'));
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (en) {
        if (en.isIntersecting && map[en.target.id]) {
          links.forEach(function (l) { l.classList.remove('on'); });
          map[en.target.id].classList.add('on');
        }
      });
    }, { rootMargin: '-80px 0px -70% 0px', threshold: 0 });
    heads.forEach(function (h) { io.observe(h); });
  }
})();
