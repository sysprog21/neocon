{
  description = "Description for the project";

  inputs = {
    flake-parts.url = "github:hercules-ci/flake-parts";
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    inputs@{ flake-parts, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      imports = [
      ];
      systems = [
        "x86_64-linux"
        "x86_64-darwin"
        "aarch64-linux"
        "arm64-darwin"
      ];
      perSystem =
        {
          config,
          pkgs,
          ...
        }:
        {
          devShells.default =
            with pkgs;
            mkShell {
              buildInputs = [
              ];
              nativeBuildInputs = [
                gcc
                gnumake
              ];
            };
          packages.default =
            with pkgs;
            callPackage (
              {
                gcc,
                gnumake,
              }:
              stdenv.mkDerivation {
                name = "neocon";
                src = ./.;

                nativeBuildInputs = [
                  gcc
                  gnumake
                ];

                makeFlags = [ "PREFIX=$(out)" ];
              }
            ) { };
        };
      flake = {
      };
    };
}
