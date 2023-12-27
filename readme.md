# Install Instructions
Set up virtual enviroment
```
python3 -m venv venv`
source venv/bin/activate
pip install conan
```

Set up conan
```
conan profile detect --force
conan install . --output-folder=build --build=missing
```

Build
```
chmod +x build.sh
./build.sh
```