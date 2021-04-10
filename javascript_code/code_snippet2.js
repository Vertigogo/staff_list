
// initialize
window.onload = function() {
    // add action to the file input
    var input = document.getElementById('file');
    input.addEventListener('change', importImage);

    // add action to the encode button
    var encodeButton = document.getElementById('encode');
    encodeButton.addEventListener('click', encode);

    // add action to the decode button
    var decodeButton = document.getElementById('decode');
    decodeButton.addEventListener('click', decode);
};

// artificially limit the message size
var maxMessageSize = 1000;

// put image in the canvas and display it
var importImage = function(e) {
    var reader = new FileReader();

    reader.onload = function(event) {
        // set the preview
        document.getElementById('preview').style.display = 'block';
        document.getElementById('preview').src = event.target.result;

        // wipe all the fields clean
        document.getElementById('message').value = '';
        document.getElementById('password').value = '';
        document.getElementById('password2').value = '';
        document.getElementById('messageDecoded').innerHTML = '';

        // read the data into the canvas element
        var img = new Image();
        img.onload = function() {
            var ctx = document.getElementById('canvas').getContext('2d');
            ctx.canvas.width = img.width;
            ctx.canvas.height = img.height;
            ctx.drawImage(img, 0, 0);

            decode();
        };
        img.src = event.target.result;
    };

    reader.readAsDataURL(e.target.files[0]);
};

// encode the image and save it
var encode = function() {
    var message = document.getElementById('message').value;
    var password = document.getElementById('password').value;
    var output = document.getElementById('output');
    var canvas = document.getElementById('canvas');
    var ctx = canvas.getContext('2d');

    // encrypt the message with supplied password if necessary
    if (password.length > 0) {
        message = sjcl.encrypt(password, message);
    } else {
        message = JSON.stringify({'text': message});
    }

    // exit early if the message is too big for the image
    var pixelCount = ctx.canvas.width * ctx.canvas.height;
    if ((message.length + 1) * 16 > pixelCount * 4 * 0.75) {
        alert('Message is too big for the image.');
        return;
    }

    // exit early if the message is above an artificial limit
    if (message.length > maxMessageSize) {
        alert('Message is too big.');
        return;
    }