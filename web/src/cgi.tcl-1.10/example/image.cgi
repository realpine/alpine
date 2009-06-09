#!/depot/path/tclsh
# See description below.

package require cgi

cgi_eval {
    source example.tcl
    cgi_input

    describe_in_frame "raw image" "This CGI script generates a raw
    image.  The script could be much more complicated - the point is
    merely to show the framework.  (The picture is of the US National
    Prototype Kilogram. It is made of 90% platinum, 10% iridium. It
    was assigned to the US in 1889 and is periodically recertified and
    traceable to [italic "The Kilogram"] held at
    [url "Bureau International des Poids et Mesures" http://www.bipm.fr" $TOP]
    in France.)"

    # ignore the junk above this line - the crucial stuff is below

    cgi_content_type "image/jpeg"

    set fh [open $DATADIR/kg.jpg r]
    fconfigure stdout -translation binary
    fconfigure $fh    -translation binary	
    fcopy $fh stdout
    close $fh
}

