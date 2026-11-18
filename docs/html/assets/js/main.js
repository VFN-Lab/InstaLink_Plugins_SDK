document.addEventListener('DOMContentLoaded', () => {
    const htmlElem = document.documentElement;
    const langBtn = document.getElementById('lang-toggle');
    const langLabel = document.getElementById('lang-label');
    const menuBtn = document.getElementById('menu-toggle');
    const sidebar = document.getElementById('left-sidebar');
    const searchInput = document.getElementById('global-search');
    const searchResults = document.getElementById('search-results');

    const getLang = () => htmlElem.getAttribute('lang') || 'en';

    const applyLang = (lang) => {
        htmlElem.setAttribute('lang', lang);
        htmlElem.setAttribute('dir', lang === 'ar' ? 'rtl' : 'ltr');
        localStorage.setItem('instasdk_lang', lang);

        document.querySelectorAll('.lang-ar').forEach((el) => {
            el.style.display = lang === 'ar' ? '' : 'none';
        });
        document.querySelectorAll('.lang-en').forEach((el) => {
            el.style.display = lang === 'ar' ? 'none' : '';
        });

        if (langLabel) {
            langLabel.textContent = lang === 'ar' ? 'AR' : 'EN';
        }
        document.querySelectorAll('.lang-option').forEach((opt) => {
            opt.classList.toggle('active', opt.getAttribute('data-lang') === lang);
        });
        if (searchInput) {
            searchInput.placeholder = lang === 'ar' ? 'بحث...' : 'Search';
        }
    };

    const activateSection = (sectionId, scrollTop = true) => {
        if (sectionId === 'sec-permissions') sectionId = 'sec-bridge';
        const target = document.getElementById(sectionId);
        if (!target) return;

        document.querySelectorAll('.doc-section').forEach((sec) => {
            sec.classList.toggle('active', sec.id === sectionId);
        });
        document.querySelectorAll('.left-sidebar a.nav-link').forEach((link) => {
            link.classList.toggle('active', link.getAttribute('data-section') === sectionId);
        });

        // Sync top header tabs
        const tabKey = target.getAttribute('data-tab') || 'develop';
        document.querySelectorAll('.header-tab').forEach((tab) => {
            tab.classList.toggle('active', tab.getAttribute('data-tab') === tabKey);
        });

        if (sidebar) sidebar.classList.remove('mobile-open');
        if (scrollTop) {
            window.scrollTo({ top: 0, behavior: 'smooth' });
        }
        if (history.replaceState) {
            history.replaceState(null, '', '#' + sectionId);
        }
    };

    // Left sidebar links
    document.querySelectorAll('.left-sidebar a.nav-link[data-section]').forEach((link) => {
        link.addEventListener('click', (e) => {
            e.preventDefault();
            activateSection(link.getAttribute('data-section'));
        });
    });

    // Top header tabs jump to primary section for that tab
    document.querySelectorAll('.header-tab[data-target]').forEach((tab) => {
        tab.addEventListener('click', (e) => {
            e.preventDefault();
            activateSection(tab.getAttribute('data-target'));
        });
    });

    // Deep full-text search with Arabic normalization, multi-word matching, and direct anchor scroll
    const normalizeText = (value) => {
        return String(value || '')
            .toLowerCase()
            .replace(/[\u064B-\u065F\u0670\u0640]/g, '')
            .replace(/[أإآٱ]/g, 'ا')
            .replace(/ة/g, 'ه')
            .replace(/ى/g, 'ي')
            .replace(/[ؤئ]/g, (ch) => (ch === 'ؤ' ? 'و' : 'ي'))
            .replace(/\s+/g, ' ')
            .trim();
    };

    const getElementSearchText = (el) => {
        if (el.tagName === 'PRE') {
            const code = el.querySelector('code');
            return (code ? code.textContent : el.textContent).replace(/\s+/g, ' ').trim();
        }
        if (el.tagName === 'TR') {
            return Array.from(el.children)
                .map((cell) => cell.textContent.replace(/\s+/g, ' ').trim())
                .filter(Boolean)
                .join(' — ');
        }
        return el.textContent.replace(/\s+/g, ' ').trim();
    };

    const buildSearchIndex = () => {
        const entries = [];
        document.querySelectorAll('.doc-section').forEach((sec) => {
            ['en', 'ar'].forEach((langCode) => {
                const container = sec.querySelector('.lang-' + langCode);
                if (!container) return;

                const pageTitleEl = container.querySelector('h1');
                const pageTitle = pageTitleEl ? pageTitleEl.textContent.replace(/\s+/g, ' ').trim() : sec.id;
                let currentSubheading = pageTitle;

                const nodes = container.querySelectorAll('h1, h2, h3, tbody tr, p, li, pre, .callout');
                nodes.forEach((node) => {
                    if (node.closest('table') && node.tagName !== 'TR') return;
                    if (node.closest('li') && node.tagName === 'P') return;

                    const rawText = getElementSearchText(node);
                    if (!rawText) return;

                    const isHeading = node.tagName === 'H1' || node.tagName === 'H2' || node.tagName === 'H3';
                    if (node.tagName === 'H2' || node.tagName === 'H3') {
                        currentSubheading = rawText;
                    } else if (node.tagName === 'H1') {
                        currentSubheading = pageTitle;
                    }

                    const weight =
                        node.tagName === 'H1' ? 120 :
                        node.tagName === 'H2' ? 95 :
                        node.tagName === 'H3' ? 85 :
                        node.tagName === 'TR' ? 65 :
                        node.tagName === 'PRE' ? 40 : 50;

                    entries.push({
                        sectionId: sec.id,
                        lang: langCode,
                        pageTitle,
                        subheading: isHeading ? rawText : currentSubheading,
                        kind: node.tagName.toLowerCase(),
                        isHeading,
                        weight,
                        rawText,
                        normText: normalizeText(rawText),
                        normHeading: normalizeText(isHeading ? rawText : currentSubheading),
                        element: node
                    });
                });
            });
        });
        return entries;
    };

    const searchIndex = buildSearchIndex();

    const buildHighlightedSnippet = (containerEl, text, rawQuery, tokens) => {
        const clean = text.length > 130 ? text.slice(0, 130) + '...' : text;
        const lower = clean.toLowerCase();
        const terms = [rawQuery.toLowerCase(), ...tokens.map((t) => t.toLowerCase())].filter((t) => t.length >= 2);

        let matchStart = -1;
        let matchLen = 0;
        for (const term of terms) {
            const pos = lower.indexOf(term);
            if (pos !== -1) {
                matchStart = pos;
                matchLen = term.length;
                break;
            }
        }

        if (matchStart === -1) {
            containerEl.textContent = clean;
            return;
        }

        containerEl.textContent = '';
        if (matchStart > 0) {
            containerEl.appendChild(document.createTextNode(clean.slice(0, matchStart)));
        }
        const mark = document.createElement('mark');
        mark.textContent = clean.slice(matchStart, matchStart + matchLen);
        containerEl.appendChild(mark);
        if (matchStart + matchLen < clean.length) {
            containerEl.appendChild(document.createTextNode(clean.slice(matchStart + matchLen)));
        }
    };

    const makeSnippetWindow = (rawText, rawQuery, tokens) => {
        if (rawText.length <= 125) return rawText;
        const lower = rawText.toLowerCase();
        let idx = lower.indexOf(rawQuery.toLowerCase());
        if (idx === -1) {
            const normLower = normalizeText(rawText);
            idx = normLower.indexOf(normalizeText(rawQuery));
            if (idx === -1 && tokens.length > 0) {
                idx = normLower.indexOf(tokens[0]);
            }
        }
        if (idx <= 35) return rawText.slice(0, 125) + '...';
        const start = Math.max(0, idx - 35);
        return (start > 0 ? '...' : '') + rawText.slice(start, start + 125) + (start + 125 < rawText.length ? '...' : '');
    };

    const jumpToSearchResult = (match) => {
        searchResults.classList.remove('open');
        if (match.lang !== getLang()) {
            applyLang(match.lang);
        }
        activateSection(match.sectionId, false);
        requestAnimationFrame(() => {
            if (!match.element) return;
            match.element.scrollIntoView({ behavior: 'smooth', block: 'center' });
            match.element.classList.remove('search-focus-flash');
            void match.element.offsetWidth;
            match.element.classList.add('search-focus-flash');
        });
    };

    let selectedResultIdx = -1;
    let currentSearchMatches = [];

    const runSearch = () => {
        if (!searchInput || !searchResults) return;
        const rawQuery = searchInput.value.trim();
        const normQuery = normalizeText(rawQuery);
        searchResults.innerHTML = '';
        selectedResultIdx = -1;
        currentSearchMatches = [];

        if (normQuery.length < 2) {
            searchResults.classList.remove('open');
            return;
        }

        const activeLang = getLang();
        const tokens = normQuery.split(' ').filter(Boolean);

        const scorePool = (targetLang) => {
            const results = [];
            for (const item of searchIndex) {
                if (item.lang !== targetLang) continue;
                const haystack = item.normHeading + ' ' + item.normText;
                const exactInText = item.normText.includes(normQuery);
                const exactInHeading = item.normHeading.includes(normQuery);
                const allTokensMatch = tokens.every((tk) => haystack.includes(tk));

                if (!exactInText && !exactInHeading && !allTokensMatch) continue;

                let score = item.weight;
                if (exactInHeading) score += 140;
                if (exactInText) score += 70;
                if (allTokensMatch) score += tokens.length * 15;

                results.push({ item, score });
            }
            return results;
        };

        let scored = scorePool(activeLang);
        if (scored.length === 0) {
            scored = scorePool(activeLang === 'ar' ? 'en' : 'ar');
        }

        scored.sort((a, b) => b.score - a.score);

        // Deduplicate closely overlapping heading + intro paragraph hits
        const seenKeys = new Set();
        const finalMatches = [];
        for (const entry of scored) {
            const key = entry.item.sectionId + '|' + entry.item.lang + '|' + entry.item.normText;
            if (seenKeys.has(key)) continue;
            seenKeys.add(key);
            finalMatches.push(entry.item);
            if (finalMatches.length >= 30) break;
        }

        currentSearchMatches = finalMatches;

        if (finalMatches.length === 0) {
            const empty = document.createElement('div');
            empty.className = 'search-result-item';
            const msg = document.createElement('div');
            msg.className = 'search-result-snippet';
            msg.textContent = activeLang === 'ar' ? 'لا توجد نتائج مطابقة' : 'No matching results';
            empty.appendChild(msg);
            searchResults.appendChild(empty);
        } else {
            finalMatches.forEach((m, idx) => {
                const item = document.createElement('a');
                item.href = '#' + m.sectionId;
                item.className = 'search-result-item';
                item.dataset.idx = String(idx);

                const metaEl = document.createElement('div');
                metaEl.className = 'search-result-meta';
                metaEl.textContent = m.lang !== activeLang
                    ? `${m.pageTitle} (${m.lang.toUpperCase()})`
                    : m.pageTitle;

                const titleEl = document.createElement('div');
                titleEl.className = 'search-result-title';
                titleEl.textContent = m.subheading;

                item.appendChild(metaEl);
                item.appendChild(titleEl);

                if (!m.isHeading) {
                    const snippetEl = document.createElement('div');
                    snippetEl.className = 'search-result-snippet';
                    const snippetWindow = makeSnippetWindow(m.rawText, rawQuery, tokens);
                    buildHighlightedSnippet(snippetEl, snippetWindow, rawQuery, tokens);
                    item.appendChild(snippetEl);
                }

                item.addEventListener('click', (e) => {
                    e.preventDefault();
                    jumpToSearchResult(m);
                });
                searchResults.appendChild(item);
            });
        }
        searchResults.classList.add('open');
    };

    // Live Search, Keyboard Navigation & "/" shortcut
    if (searchInput && searchResults) {
        document.addEventListener('keydown', (e) => {
            if (e.key === '/' && document.activeElement !== searchInput) {
                e.preventDefault();
                searchInput.focus();
            } else if (e.key === 'Escape') {
                searchResults.classList.remove('open');
                searchInput.blur();
            }
        });

        searchInput.addEventListener('keydown', (e) => {
            if (!searchResults.classList.contains('open') || currentSearchMatches.length === 0) return;
            const items = searchResults.querySelectorAll('.search-result-item[data-idx]');
            if (e.key === 'ArrowDown') {
                e.preventDefault();
                selectedResultIdx = Math.min(currentSearchMatches.length - 1, selectedResultIdx + 1);
            } else if (e.key === 'ArrowUp') {
                e.preventDefault();
                selectedResultIdx = Math.max(0, selectedResultIdx - 1);
            } else if (e.key === 'Enter') {
                e.preventDefault();
                const pickIdx = selectedResultIdx >= 0 ? selectedResultIdx : 0;
                jumpToSearchResult(currentSearchMatches[pickIdx]);
                return;
            } else {
                return;
            }

            items.forEach((el, idx) => {
                el.classList.toggle('active', idx === selectedResultIdx);
            });
            if (items[selectedResultIdx]) {
                items[selectedResultIdx].scrollIntoView({ block: 'nearest' });
            }
        });

        searchInput.addEventListener('input', runSearch);
        searchInput.addEventListener('focus', () => {
            if (searchInput.value.trim().length >= 2) {
                runSearch();
            }
        });

        document.addEventListener('click', (e) => {
            if (!e.target.closest('.search-wrapper')) {
                searchResults.classList.remove('open');
            }
        });
    }

    // Copy buttons for code blocks
    document.querySelectorAll('pre').forEach((pre) => {
        const btn = document.createElement('button');
        btn.className = 'copy-btn';
        btn.type = 'button';
        btn.textContent = 'Copy';
        btn.addEventListener('click', async () => {
            const code = pre.querySelector('code');
            if (!code) return;
            try {
                await navigator.clipboard.writeText(code.innerText);
                btn.textContent = getLang() === 'ar' ? 'تم النسخ' : 'Copied!';
                setTimeout(() => { btn.textContent = 'Copy'; }, 1500);
            } catch {
                btn.textContent = 'Error';
            }
        });
        pre.appendChild(btn);
    });

    // Language & Sidebar toggles
    const langMenu = document.getElementById('lang-menu');
    if (langBtn && langMenu) {
        langBtn.addEventListener('click', (e) => {
            e.stopPropagation();
            const isOpen = langMenu.classList.toggle('open');
            langBtn.setAttribute('aria-expanded', String(isOpen));
        });

        langMenu.querySelectorAll('.lang-option').forEach((opt) => {
            opt.addEventListener('click', () => {
                const chosen = opt.getAttribute('data-lang') || 'en';
                applyLang(chosen);
                if (searchInput && searchInput.value.trim().length >= 2) {
                    runSearch();
                }
                langMenu.classList.remove('open');
                langBtn.setAttribute('aria-expanded', 'false');
            });
        });

        document.addEventListener('click', (e) => {
            if (!e.target.closest('.lang-dropdown')) {
                langMenu.classList.remove('open');
                langBtn.setAttribute('aria-expanded', 'false');
            }
        });
    }

    if (menuBtn && sidebar) {
        menuBtn.addEventListener('click', () => {
            if (window.innerWidth <= 860) {
                sidebar.classList.toggle('mobile-open');
            } else {
                document.body.classList.toggle('sidebar-collapsed');
            }
        });
    }

    // Keep default light theme and restore saved language
    htmlElem.setAttribute('data-theme', 'light');
    localStorage.removeItem('instasdk_theme');
    applyLang(localStorage.getItem('instasdk_lang') || 'en');

    const initialHash = window.location.hash ? window.location.hash.slice(1) : 'sec-overview';
    const resolvedHash = initialHash === 'sec-permissions' ? 'sec-bridge' : initialHash;
    activateSection(document.getElementById(resolvedHash) ? resolvedHash : 'sec-overview');
});
