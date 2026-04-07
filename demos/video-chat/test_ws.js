const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ headless: false });
  const context = await browser.newContext();
  const page = await context.newPage();
  
  // Capture all console messages
  page.on('console', msg => {
    console.log(`[Browser ${msg.type()}]: ${msg.text()}`);
  });
  
  // Capture WebSocket traffic
  page.on('websocket', ws => {
    console.log(`[WebSocket] Connected: ${ws.url()}`);
    
    ws.on('framesent', data => {
      console.log(`[WebSocket] Sent: ${data.payload}`);
    });
    
    ws.on('framereceived', data => {
      console.log(`[WebSocket] Received: ${data.payload}`);
    });
    
    ws.on('close', () => {
      console.log('[WebSocket] Closed');
    });
  });
  
  // Navigate to the video chat
  await page.goto('http://localhost:8897');
  
  console.log('Page loaded, creating a room...');
  
  // Wait for the page to be ready
  await page.waitForTimeout(1000);
  
  // Click "Create Room" button
  await page.click('text=Create Room');
  
  // Wait and observe
  await page.waitForTimeout(5000);
  
  console.log('Test complete, keeping browser open for inspection...');
  
  // Keep browser open
  await new Promise(() => {});
})();
