{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        buildInputs = with pkgs; [
          # Build tools
          cmake
          gnumake
          gcc
          pkg-config

          # libs
          sdl3
          sdl3-image
          ffmpeg
          taglib_1
          curl
          libvlc
        ];

        # Optional: set any env vars your project needs
        shellHook = ''
          echo "C++ dev environment ready"
        '';
      };
    };
}
