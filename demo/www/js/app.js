document.addEventListener("DOMContentLoaded", function() {
    var heading = document.querySelector("h1");
    if (heading) {
        heading.style.cursor = "pointer";
        heading.addEventListener("click", function() {
            heading.textContent = "Resources loaded from memory!";
        });
    }
    console.log("cres demo: all resources served from embedded binary data");
});
