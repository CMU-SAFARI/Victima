FROM python:3.9
WORKDIR /app
COPY ./ /app/

RUN apt-get update
RUN apt-get install -y libsystemd-dev gcc python3-dev pkg-config libdbus-1-dev
RUN pip3 install  --upgrade pip
RUN pip3 --version
RUN pip3 install torch torchvision torchaudio --index-url  https://download.pytorch.org/whl/cpu
RUN pip3 install torchmetrics
RUN pip3 install pandas numpy matplotlib seaborn
CMD ["python3", "predict.py"]
