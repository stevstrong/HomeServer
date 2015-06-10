<?php
/*****************************************************************************/
// list of VITO data
/*****************************************************************************/
$frameReadings = [
//	'Gerät-Info'			=> { 'addr' => 0x00F8, 'bytes' => 2, 'divider' =>  1, 'unp' => 's' },
	'Temp-Aussen'			=> [ 'addr' => 0x0101, 'bytes' => 2, 'divider' => 10, 'unp' => 'i' ],
	'Temp-Luft-Ein'			=> [ 'addr' => 0x0103, 'bytes' => 2, 'divider' => 10, 'unp' => 'i' ],
	'Temp-Luft-Aus'			=> [ 'addr' => 0x0104, 'bytes' => 2, 'divider' => 10, 'unp' => 'i' ],
	'Temp-Hzg-VL'			=> [ 'addr' => 0x0105, 'bytes' => 2, 'divider' => 10, 'unp' => 'i' ],
	'Temp-Hzg-RL'			=> [ 'addr' => 0x0106, 'bytes' => 2, 'divider' => 10, 'unp' => 'i' ],
	'Temp-WW'				=> [ 'addr' => 0x010D, 'bytes' => 2, 'divider' => 10, 'unp' => 'i' ],
	'Status-Verdichter'		=> [ 'addr' => 0x0400, 'bytes' => 1, 'divider' =>  1, 'unp' => 'b' ],
	'Status-Pumpe-Hzg'		=> [ 'addr' => 0x040D, 'bytes' => 1, 'divider' =>  1, 'unp' => 'b' ],
	'Status-Pumpe-WW'		=> [ 'addr' => 0x0414, 'bytes' => 1, 'divider' =>  1, 'unp' => 'b' ],
#
#	'Temp-WW-Soll'			=> [ 'addr' => 0x0020, 'bytes' => 4, fmat => 10, unp => 'f<' },
	'Verdichter-BetriebStd'	=> [ 'addr' => 0x0580, 'bytes' => 4, 'divider' => 10, 'unp' => 'i' ],
	'Verdichter-NrStarts'	=> [ 'addr' => 0x0500, 'bytes' => 4, 'divider' =>  1, 'unp' => 'i' ],
	'WP-Uhrzeit'			=> [ 'addr' => 0x08E0, 'bytes' => 8, 'divider' =>  1, 'unp' => 's' ],
#
	'Strom-WP'				=> [ 'addr' => 0xFFFE, 'bytes' => 2, 'divider' =>  1, 'unp' => 'i' ],
	'Strom-H'				=> [ 'addr' => 0xFFFF, 'bytes' => 2, 'divider' =>  1, 'unp' => 'i' ],
	'Strom-Datum'			=> [ 'addr' => 0xFFFF, 'bytes' => 4, 'divider' =>  1, 'unp' => 's' ],
];

$readStatus = ['Temp-Aussen','Temp-Hzg-VL','Temp-Hzg-RL','Temp-WW',
				'Status-Verdichter','Status-Pumpe-Hzg','Status-Pumpe-WW','Strom-H','Strom-WP'];
/*****************************************************************************/
/*****************************************************************************/
function BuildSendCommand($fp, $cmd, $key, $value)
{
global $frameReadings;

	set_time_limit(5);
	// enter cmd mode: send 16 00 00 as long as the return byte is 06
	$i = 5;
	do {
		fwrite($fp, pack('H*', "160000"), 3);
		$char0 = ord( fgetc($fp) );
	} while ( $char0!=0x06 and (--$i)>0 );
	if ($i==0)
	{	// error occured
		fclose ($fp);
		die ("ERROR: Polling could not be stopped, received: $char0");
	}
	// so far so good. build here the string
	//check if $key exists
	if ( !isset($frameReadings[$key]) )
		die ("ERROR: BuildSendCommand: parameter $key not found!");
	$addr = $frameReadings[$key]['addr'];
	$byts = $frameReadings[$key]['bytes'];
	if ($cmd == "read")
	{
		$frame = chr(0x05) . chr(0x00) . chr(0x01) . chr($addr>>8) . chr($addr%256) . chr($byts);
	}
	elseif ($cmd == "write")
	{
		$frame = chr(5+$byts) . chr(0x00) . chr(0x02) . chr($addr>>8) . chr($addr%256) . chr($byts);// . ;
		$frame .= ParseWrValue($key, $value);	// done!
	//	echo "This command is not yet supported!\n";
	}
	else
	{
		echo "BuildSendCommand: undefined command '$cmd'!";
		return undef;
	}
	$out = chr(0x41) . $frame . chr(CRC8($frame));	// add cmdID as first and crc as last element
/**/
/*	$out = "";
	/* convert the array to binary data string
	foreach ($frame as &$value)
		$out .= chr($value);
*/
	// send command string
	fwrite($fp, $out, strlen($out));

	return strlen($out);
}

/*****************************************************************************/
/*****************************************************************************/
function CRC8($st)
{
	for ($i=0, $crc=0, $len=strlen($st); $len>0; $i++, $len--)
	{
		$crc += ord($st[$i]);
		//echo "byte $i: ".ord($st[$i]).", crc: ". $crc."<br>";
	}
	$crc %= 256;
	return ( $crc );	// only LSB is needed
}

/*****************************************************************************/
/*****************************************************************************/
function ReceiveData($fp)
{
	set_time_limit(5);

//	echo "Getting: ";
	for ($i = 0, $len = 5, $crc = 0, $in = "";
												$len>0;
														++$i, --$len)
	{	// read single characters until size (byte 2 of the received stream) is reached
		$char0 = ord( fgetc($fp) );	// take the numeric value of the received character
		//var_dump($char0);
		//echo "Answer byte nr. $i is: $char0,\t\thex: ". bin2hex(chr(($char0))) . "<br>"; // print one byte
//		echo bin2hex(chr(($char0))) . " "; // print one byte
		if ($i==0)
		{	// check if the first byte is 06
			if ( $char0==0x41 )
				$i = 1;
			elseif ( $char0!=0x06 ) {
				echo "First byte ".$char0.", hex: ". dechex($char0) . " unknown, expected '0x06'!\n";
				continue;
			}
		}
		elseif ( $i==2 )
		{	// check size byte
			$len = $char0+2;
			//echo "Data size is: $char0<br>";
		}
		$in .= chr($char0);	// build answer string here, except byte 0
		if ($i>1 and $len>1)
			$crc += $char0;	// calculate crc, except bytes 0, 1 and last one
	}
	$crc %= 256;
//	echo "<br>";
	//echo "CRC is: $crc\t";
	// check CRC error
	if ( $crc!=$char0 )	// last received byte
	{	echo "\nGot CRC error for the incoming string '$in'!\n"; return 0;}

	return $in;
}

/*****************************************************************************/
/*****************************************************************************/
function ParseRecData($rs, $key)
{
global $frameReadings;
	set_time_limit(5);

//	$res = unpack('I*', $rs);
//	var_dump($res); echo "<br>"; return;
	$res = unpack('H*', $rs);
//	var_dump($res[1]);	echo "<br>";
//	echo "$rs <br>";

	$regx = "~41([\da-f]{2})([\da-f]{2})01([\da-f]{4})([\da-f]{2})([\da-f]*)[\da-f]{2}~";
	
	$m = preg_match($regx, $res[1], $matches);
//	var_dump( $m );	echo "<br>";
//	var_dump( $matches );	echo "<br>";
	if ( !$m )
		echo "No match found, this data could not be parsed: " . $res[1] . "\n";

	// command "list" only woks on numerical arrays, starting with index 0
	list( , $len, $err, $ad, $byts, $data) = $matches;
//	var_dump( $data );	echo "<br>";

//	var_dump( $frameReadings[$key] ); 	echo " =key<br>";
	// to extract non-numerical array key values use the function 'extract()'
	$addr = "Original";	// needed to extract the original key values into the symbol table
	extract($frameReadings[$key]);
//	echo "\$a = $addr; \$b = $bytes; \$c = $divider; \$d = $unp<br>";
/* workaround for wrong bytes returned from ATTiny */
	if ( ($addr&0xfffe)==0xfffe )
		$byts = 2;

	if ( $unp=='i' )	// integer
	{
		if ( $byts==2 )		$ret = Parse2Bytes($data);
		elseif ( $byts==4 )	$ret = Parse4Bytes($data);
		// divide with the divider
		$ret /= $divider;
	}
	elseif ( $unp=='b' )	// binary
	{
		if ( $data=='00' )
			$ret = '0';
		else
			$ret = '1';
	}
	elseif ( $unp=='s' )	// string
	{
		$ret = $data;
	}

return $ret;
}

/*****************************************************************************/
/*****************************************************************************/
function Parse2Bytes($st)
{	// ABCD -> CDAB
	$a = intval($st[2].$st[3].$st[0].$st[1], 16);
	if ( $a & 0x8000 ) $a = $a - 0x10000;	// resolve sign
	return ( $a );
}

/*****************************************************************************/
/*****************************************************************************/
function Parse4Bytes($st)
{	// ABCDEFGH -> GHEFCDAB
	$a = intval($st[6].$st[7].$st[4].$st[5].$st[2].$st[3].$st[0].$st[1], 16);
	if ( $a & 0x80000000 ) $a = $a - 0x100000000;	// resolve sign
	return ( $a );
}

/*****************************************************************************/
/*****************************************************************************/
function ParseWrValue($key, $val)
{
global $frameReadings;
	set_time_limit(5);

	// to extract non-numerical array key values use the function 'extract()'
	$addr = "Original";	// needed to extract the original key values into the symbol table
	extract($frameReadings[$key]);
//	echo "\$addr = $addr; \$bytes = $bytes; \$divider = $divider; \$unp = $unp\n";

	if ( $unp=='i' )	// integer
	{
		// multiply by the divider
		$val *= $divider;
		if ( $byts==2 )		$ret = Build2Bytes((int)$data);
		elseif ( $byts==4 )	$ret = Build4Bytes((int)$data);
	}
	elseif ( $unp=='b' )	// binary
	{
		$ret = chr($val);
	}
	elseif ( $unp=='s' )	// string
	{
		$ret = BuildStringValue($val);
	}

return $ret;
}

/*****************************************************************************/
/*****************************************************************************/
function Build2Bytes($dt)
{
	return (chr($dt>>8) . chr($dt%256));
}

/*****************************************************************************/
/*****************************************************************************/
function Build4Bytes($dt)
{
	return Build2Bytes($dt>>16) . chr($dt%65536);
}

/*****************************************************************************/
/*****************************************************************************/
function BuildStringValue($dt)
{
	$dtlen = strlen($dt);
	$ret = '';
	for ($i=0; $i<$dtlen/2; $i++)
		$ret .= chr(hexdec($dt[2*$i])*16+hexdec($dt[2*$i+1]));
	return $ret;
}

/*****************************************************************************/
/*
utils:
	base_convert($bin,2,10); 
/*****************************************************************************/
?>




