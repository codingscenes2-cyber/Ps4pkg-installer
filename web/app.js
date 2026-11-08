const urlInput=document.querySelector("#url");
const sendButton=document.querySelector("#send");
const message=document.querySelector("#message");
sendButton.addEventListener("click",async()=>{
  const url=urlInput.value.trim();
  if(!/^https?:\/\//i.test(url)){message.textContent="Enter a direct HTTP(S) URL.";return;}
  // The authenticated local API is intentionally not stubbed as a success.
  // Pairing token + service endpoint will be wired in the server milestone.
  message.textContent="Console service is not connected in this scaffold.";
});
