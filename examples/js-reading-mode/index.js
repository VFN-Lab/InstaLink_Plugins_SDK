// Focus Reading Mode Plugin
(function() {
    console.log("Reading Mode Plugin Loaded");
    
    function applyReadingMode() {
        // Attempt to find the main article content
        const article = document.querySelector('article') || document.querySelector('.main-content') || document.querySelector('main');
        
        if (article) {
            // Hide everything else
            document.body.innerHTML = '';
            
            // Re-add the article with custom styling
            const container = document.createElement('div');
            container.style.maxWidth = '800px';
            container.style.margin = '0 auto';
            container.style.padding = '40px 20px';
            container.style.fontFamily = 'Georgia, serif';
            container.style.fontSize = '20px';
            container.style.lineHeight = '1.8';
            container.style.backgroundColor = '#fdfdfd';
            container.style.color = '#222';
            
            container.appendChild(article);
            document.body.appendChild(container);
            document.body.style.backgroundColor = '#f0f0f0';
        }
    }

    // Give the page a little time to load dynamic content
    setTimeout(applyReadingMode, 2000);
})();
