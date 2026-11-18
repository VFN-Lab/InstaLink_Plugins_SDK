// This script is injected into web pages
(function() {
    "use strict";

    console.log("[InstaLink Extension] Content script loaded on: " + window.location.href);

    // Add a visual badge to the page
    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", addBadge);
    } else {
        addBadge();
    }

    function addBadge() {
        const badge = document.createElement("div");
        badge.className = "instalink-injected-badge";
        badge.textContent = "✓ InstaLink Extension Active";
        document.body.appendChild(badge);

        // Remove it after 5 seconds
        setTimeout(() => {
            badge.style.opacity = '0';
            badge.style.transition = 'opacity 0.5s ease';
            setTimeout(() => badge.remove(), 500);
        }, 5000);
    }

})();


