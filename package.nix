{
  stdenv,
  gnumake,
  gcc,
}:

stdenv.mkDerivation {
  pname = "acush";
  version = "1.0.0";

  src = ./.;

  nativeBuildInputs = [
    gnumake
    gcc
  ];

  installPhase = ''
    mkdir -p "$out/bin"
    cp "build/acush" "$out/bin"
  '';
}
