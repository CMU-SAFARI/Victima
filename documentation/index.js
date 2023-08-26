const tabButtons = document.querySelectorAll('.tab-button');
const tabItems = document.querySelectorAll('.tab-item');

tabButtons.forEach(button => {
  button.addEventListener('click', () => {
    const targetTab = button.dataset.tab;

    // Deactivate all buttons and items
    tabButtons.forEach(btn => btn.classList.remove('active'));
    tabItems.forEach(item => item.classList.remove('active'));

    // Activate the clicked button
    button.classList.add('active');

    // Load content for the clicked tab
    loadTabContent(targetTab);
  });
});

function loadTabContent(tabId) {
  const tabContent = document.getElementById(tabId);
  if (tabContent.innerHTML.trim() === '') {
    // Fetch HTML content for the tab using AJAX
    const xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function () {
      if (xhr.readyState === XMLHttpRequest.DONE) {
        if (xhr.status === 200) {
          tabContent.innerHTML = xhr.responseText;
          tabContent.classList.add('active');
        } else {
          console.error('Failed to load content for tab:', tabId);
        }
      }
    };

    xhr.open('GET', `${tabId}.html`, true);
    xhr.send();
  } else {
    // If content is already loaded, just activate the tab
    tabContent.classList.add('active');
  }
}
