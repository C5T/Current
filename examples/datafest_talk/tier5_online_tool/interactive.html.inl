<!doctype html>

<html>

<head>
  <title>1GBPS++ Crunching Script Editor</title>
</head>

<body>

  <h3>Script</h3>

  <textarea form ="code_form_id" name="code" id="code_textarea_id" cols="120" rows="25" wrap="soft"></textarea>
  <br>
  <br>
  <form method="get" id="code_form_id">
    <input type="submit" value="Run" />
  </form>

  <div id="link_div_id">
    <h3>Endpoint</h3>
    <a href="" id="link_href_id">Link</a>
  </div>

  <div id="result_div_id">
    <h3>Result</h3>
    <pre id="result_id"></pre>
  </div>

  <div id="error_div_id">
    <h3>Error</h3>
    <pre id="error_id"></pre>
  </div>

</body>

<script>

var json = ${CODE_RESULT_ERROR};

document.getElementById("code_textarea_id").innerHTML = json.code;

if (json.id && !json.error) {
  document.getElementById("link_href_id").setAttribute("href", json.id);
} else {
  document.getElementById("link_div_id").style.display = "none";
}

if (json.result) {
  document.getElementById("result_id").innerText = json.result;
} else {
  document.getElementById("result_div_id").style.display = "none";
}

if (json.error) {
  document.getElementById("error_id").innerText = json.error;
} else {
  document.getElementById("error_div_id").style.display = "none";
}

</script>

</html>
