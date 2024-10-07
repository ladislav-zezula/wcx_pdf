rule pdf_sample_ascii85_decode
{
	strings:
		$s00 = "%PDF-1."
		$s01 = "/ASCII85Decode"
	condition:
		$s00 at 0 and
		all of them
}

rule pdf_sample_asciihex_decode
{
	strings:
		$s00 = "%PDF-1."
		$s01 = "/ASCIIHexDecode"
	condition:
		$s00 at 0 and
		all of them
}

rule pdf_sample_lzw_decode
{
	strings:
		$s00 = "%PDF-1."
		$s01 = "/LZWDecode"
	condition:
		$s00 at 0 and
		all of them
}

rule pdf_sample_run_length_decode
{
	strings:
		$s00 = "%PDF-1."
		$s01 = "/RunLengthDecode"
	condition:
		$s00 at 0 and
		all of them
}
