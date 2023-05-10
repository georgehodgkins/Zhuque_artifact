# 3DXPoint Graphs

## multiplot.py

Create virtual environment:
```
python3 -m venv venv-multiplot
```

Activate venv-multiplot:
```
source venv-multiplot/bin/activate
```

Install packages for multiplot.py:
```
(venv-multiplot) pip install --upgrade pip
(venv-multiplot) pip install -r multiplot.deps
```

Make graphs with a config name defined in the specified YAML file.
```
(venv-multiplot) ./multiplot -y <yaml> -c <config>
```

Examples:
```
(venv-multiplot) ./multiplot.py -y multiplot-examples/sosp.yml -c fileops
(venv-multiplot) ./multiplot.py --yaml multiplot-examples/sosp.yml --config fio
```

Check default rcParams:
```
(venv-multiplot) python3 -c "import matplotlib; print(matplotlib.rcParams)"
```

Deactivate virtual environment:
```
(venv-multiplot) deactivate
```

Delete virtual environment:
```
rm -r venv-multiplot
```
