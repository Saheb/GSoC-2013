<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Frameset//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-frameset.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/xhtml;charset=UTF-8"/>
<title>ogdf</title>
</head>
<frameset cols="250,*">
  <frame src="tree.html" name="treefrm"/>
  <?php
    $mypage = $_GET["page"];
    if ($mypage == '')
      $mypage = 'main.html';
    echo '<frame src="' . $mypage . '" name="basefrm"/>';
  ?>
  <noframes>
    <body>
    <a href="main.html">Frames are disabled. Click here to go to the main page.</a>
    </body>
  </noframes>
</frameset>
</html>
