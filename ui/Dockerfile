FROM python:3.11-slim

WORKDIR /app
# only UI package needed at runtime
COPY ui/ /app/ui/

ENV GHAR_RESULTS=/data/results
ENV GHAR_UI_HOST=0.0.0.0
ENV GHAR_UI_PORT=8765
ENV PYTHONUNBUFFERED=1

EXPOSE 8765
CMD ["python3", "/app/ui/app.py"]
