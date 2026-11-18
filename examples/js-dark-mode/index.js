// Dark Mode Enforcer Plugin
(function() {
    console.log("Dark Mode Plugin Loaded");
    
    function applyDarkMode() {
        const style = document.createElement('style');
        style.type = 'text/css';
        style.innerHTML = `
            html, body {
                background-color: #121212 !important;
                color: #e0e0e0 !important;
            }
            a {
                color: #bb86fc !important;
            }
            img, video {
                filter: brightness(0.8) contrast(1.2);
            }
            div, p, span, header, footer, section, article {
                background-color: transparent !important;
                color: #e0e0e0 !important;
                border-color: #333 !important;
            }
        `;
        document.head.appendChild(style);
    }

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", applyDarkMode);
    } else {
        applyDarkMode();
    }
})();
