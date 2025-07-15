{
  inputs = {
    flake-utils.url = "github:Numtide/flake-utils";
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    typix = {
      url = "github:loqusion/typix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    {
      self,
      flake-utils,
      nixpkgs,
      typix,
      ...
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        inherit (pkgs) lib;

        typixLib = typix.lib.${system};

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

        docsSrc = lib.fileset.toSource {
          root = ./docs;
          fileset = lib.fileset.unions [
            (lib.fileset.fromSource (typixLib.cleanTypstSource ./docs))
            ./docs/graphics
          ];
        };

        docsCommonArgs = {
          typstSource = "docs.typ";
        };

        docs = typixLib.buildTypstProject (docsCommonArgs // { src = docsSrc; });
        build-docs = typixLib.buildTypstProjectLocal (docsCommonArgs // { src = docsSrc; });
        watch-docs = typixLib.watchTypstProject { typstSource = "docs/docs.typ"; };
      in
      {
        devShells.default = typixLib.devShell {
          packages = with pkgs; [
            gcc
            gnumake
            valgrind
            clang-tools
            bear
            include-what-you-use

            build-docs
            watch-docs
          ];
        };

        checks = {
          inherit
            acush
            docs
            build-docs
            watch-docs
            ;
        };

        packages = {
          inherit
            acush
            docs
            ;
          default = acush;
        };

        apps =
          let
            acushApp = flake-utils.lib.mkApp {
              drv = self.packages.${system}.default;
            };
          in
          {
            acush = acushApp;
            default = acushApp;

            build-docs = flake-utils.lib.mkApp {
              drv = build-docs;
            };

            watch-docs = flake-utils.lib.mkApp {
              drv = watch-docs;
            };
          };
      }
    );
}
