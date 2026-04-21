{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  name = "voxeterra-env";

  buildInputs = with pkgs; [
    # Core development tools
    gcc
    gnumake
    cmake
    pkg-config
    
    # Graphics and windowing
    vulkan-loader
    vulkan-headers
    vulkan-tools
    vulkan-validation-layers
    shaderc         
    
    glfw
    
    # Math library
    glm
    
    # Debugging
    gdb
    valgrind
    git

    # Wayland
    wayland
    wayland-protocols
    libxkbcommon    

    # X11
    xorg.libX11
    xorg.libXi
    xorg.libXrandr
    xorg.libXcursor
    xorg.libXinerama
    xorg.libXext
    xorg.libXfixes
  ];

  shellHook = ''
    exec zsh
    export VULKAN_SDK=${pkgs.vulkan-headers}
    export VK_LAYER_PATH=${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d
    export LD_LIBRARY_PATH=/run/opengl-driver/lib:${pkgs.vulkan-loader}/lib:$LD_LIBRARY_PATH
    export XDG_DATA_DIRS=/run/opengl-driver/share:$XDG_DATA_DIRS
    echo Start nix-shell
  '';
}
