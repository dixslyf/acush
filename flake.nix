{
  inputs = {
    flake-utils.url = "github:Numtide/flake-utils";
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs =
    {
      self,
      flake-utils,
      nixpkgs,
      ...
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        acush = pkgs.stdenv.mkDerivation {
          pname = "acush";
          version = "1.0.0";

          src = ./.;

          nativeBuildInputs = with pkgs; [
            gnumake
            gcc
          ];

          installPhase = ''
            mkdir -p "$out/bin"
            cp "build/acush" "$out/bin"
          '';
        };
      in
      {
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            gcc
            gnumake
            valgrind
            clang-tools
            bear
            include-what-you-use

            typst
          ];
        };

        packages = {
          inherit acush;
          default = acush;
        };

        apps =
          let
            acushApp = {
              type = "app";
              program = "${self.packages.${system}.default}/bin/acush";
            };
          in
          {
            acush = acushApp;
            default = acushApp;
          };
      }
    );
}
