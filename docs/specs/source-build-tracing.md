# source tarball build tracing

When a mmpack source tarball is created, a file holding traceability
information is recorded during the creation process. This file named
**mmpack/src_orig_tracing** will hold information helping to reproduce a source
tarball and possibly link the result to a specific version in the version
control system or a specific tarball release (depending on which is build
process used).

The generated data consist in a dictionary written in YAML format whose the
fields 'packaging' and 'upstream' hold information about respectively the
packaging source and the upstream source code.

## Source package traceability

The 'packaging' field consists in simple dictionary holding key/value
describing the origin of the mmpack packaging information. It will always
consist of at least the 'method' field and other field that depends on the
method used.

 * method: the value can be one of the following strings:
   - 'git': a git checkout has been used to generate the mmpack source packaging
   - 'tar': a tarball containing the source of packaging has been extracted.

### git method

 * ref: full SHA-1 of the git commit used to generate the source tarball
 * url: URL of the repository used to fetch the sources

### tar method

 * filename: basename of the tarball unpacked to get mmpack packaging sources
 * sha256: SHA-256 of the extracted tarball.


## Upstream source code origin traceability

As for mmpack packaging sources, the 'upstream' field consists in simple
dictionary holding key/value describing the origin of the mmpack packaging
information. It will always consist of at least the 'method' field and other
field that depends on the method used.

 * method: the value can be one of the following strings:
   - 'in-src-pkg': the mmpack packaging was provided along with the project
     source code. No sources-strap file was present.
   - 'git': a git checkout has been used to fetch the upstream sources.
   - 'tar': a tarball containing the upstream source code has been downloaded.

### git method

 * ref: full SHA-1 of the git commit used to generate the source tarball
 * url: public URL of the repository used to fetch the sources

### tar method

 * url: public url used to download the upstream tarball.
 * sha256: SHA-256 of the downloaded upstream tarball.
